#!/usr/bin/env bash
# AP M5-1.6 Tranche D: minimal fake `gh` CLI for the release dry-run.
# Intercepts the four `gh release` subcommands the release workflow uses
# (view, create, edit, upload) and stores release state on disk under
# $XRAY_FAKE_GH_DIR. The dry-run orchestrator (release-dry-run.sh) puts a
# symlink to this script under $XRAY_FAKE_GH_BIN/gh and exports
# $XRAY_FAKE_GH_BIN at the front of PATH so subsequent `gh` calls land
# here instead of the real CLI. Calls outside the four supported
# subcommands fail loud so the dry-run cannot accidentally hit a real
# endpoint.
#
# State layout under $XRAY_FAKE_GH_DIR:
#   releases/<tag>/metadata.json   one record per release (tag, title,
#                                  notes, draft flag, sha256 of each
#                                  uploaded asset)
#   releases/<tag>/assets/<name>   the uploaded asset bytes (one file per
#                                  asset)
#
# State is plain files instead of an in-memory structure so each gh
# invocation is independent (the workflow runs `gh` from one shell and
# can call it multiple times). jq is used for JSON manipulation; the
# dry-run setup checks jq availability up front.
set -euo pipefail

if [ -z "${XRAY_FAKE_GH_DIR:-}" ]; then
    echo "fake-gh: error: XRAY_FAKE_GH_DIR must be set" >&2
    exit 2
fi

if ! command -v jq >/dev/null 2>&1; then
    echo "fake-gh: error: jq is required" >&2
    exit 2
fi

releases_dir="$XRAY_FAKE_GH_DIR/releases"
mkdir -p "$releases_dir"

if [ $# -lt 2 ]; then
    echo "fake-gh: error: expected at least <command> <subcommand>" >&2
    exit 2
fi

cmd="$1"
sub="$2"
shift 2

if [ "$cmd" != "release" ]; then
    echo "fake-gh: error: unsupported gh subcommand '$cmd' (only 'release' is faked)" >&2
    exit 2
fi

release_dir_for_tag() {
    local tag="$1"
    printf '%s' "$releases_dir/$tag"
}

require_existing_release() {
    local tag="$1" subcmd="$2"
    local rel_dir
    rel_dir="$(release_dir_for_tag "$tag")"
    if [ ! -d "$rel_dir" ]; then
        echo "fake-gh: error: release '$tag' does not exist (gh release $subcmd)" >&2
        exit 1
    fi
}

asset_sha256() {
    sha256sum "$1" | awk '{print $1}'
}

cmd_view() {
    local tag="$1"
    local rel_dir
    rel_dir="$(release_dir_for_tag "$tag")"
    if [ ! -f "$rel_dir/metadata.json" ]; then
        echo "fake-gh: release '$tag' not found" >&2
        exit 1
    fi
    cat "$rel_dir/metadata.json"
}

cmd_create() {
    local tag="$1"
    shift
    local title="" notes_file="" draft="false"
    local -a asset_paths=()
    while [ $# -gt 0 ]; do
        case "$1" in
            --verify-tag) shift ;;
            --draft) draft="true"; shift ;;
            --title) title="$2"; shift 2 ;;
            --title=*) title="${1#--title=}"; shift ;;
            --notes-file) notes_file="$2"; shift 2 ;;
            --notes-file=*) notes_file="${1#--notes-file=}"; shift ;;
            --*)
                echo "fake-gh: error: unsupported flag '$1' for 'release create'" >&2
                exit 2 ;;
            *) asset_paths+=("$1"); shift ;;
        esac
    done

    local rel_dir
    rel_dir="$(release_dir_for_tag "$tag")"
    if [ -d "$rel_dir" ]; then
        echo "fake-gh: error: release '$tag' already exists" >&2
        exit 1
    fi
    mkdir -p "$rel_dir/assets"

    local notes=""
    if [ -n "$notes_file" ]; then
        if [ ! -f "$notes_file" ]; then
            echo "fake-gh: error: notes-file '$notes_file' missing" >&2
            exit 1
        fi
        notes="$(cat "$notes_file")"
    fi

    local assets_json="[]"
    local asset
    for asset in "${asset_paths[@]}"; do
        if [ ! -f "$asset" ]; then
            echo "fake-gh: error: asset '$asset' missing" >&2
            exit 1
        fi
        local name
        name="$(basename "$asset")"
        cp "$asset" "$rel_dir/assets/$name"
        local sha
        sha="$(asset_sha256 "$asset")"
        assets_json="$(jq --arg name "$name" --arg sha "$sha" \
            '. + [{"name": $name, "sha256": $sha}]' <<<"$assets_json")"
    done

    jq -n --arg tag "$tag" --arg title "$title" --arg notes "$notes" \
          --arg draft "$draft" --argjson assets "$assets_json" \
          '{tag: $tag, title: $title, notes: $notes, draft: ($draft == "true"), assets: $assets}' \
        > "$rel_dir/metadata.json"
    echo "fake-gh: created release $tag (draft=$draft, ${#asset_paths[@]} asset(s))"
}

cmd_edit() {
    local tag="$1"
    shift
    require_existing_release "$tag" edit

    local notes_file="" draft_flag=""
    while [ $# -gt 0 ]; do
        case "$1" in
            --notes-file) notes_file="$2"; shift 2 ;;
            --notes-file=*) notes_file="${1#--notes-file=}"; shift ;;
            --draft=*) draft_flag="${1#--draft=}"; shift ;;
            --draft) draft_flag="true"; shift ;;
            --*)
                echo "fake-gh: error: unsupported flag '$1' for 'release edit'" >&2
                exit 2 ;;
            *) echo "fake-gh: error: unexpected positional '$1' for 'release edit'" >&2; exit 2 ;;
        esac
    done

    local rel_dir
    rel_dir="$(release_dir_for_tag "$tag")"
    local meta="$rel_dir/metadata.json"
    if [ -n "$notes_file" ]; then
        if [ ! -f "$notes_file" ]; then
            echo "fake-gh: error: notes-file '$notes_file' missing" >&2
            exit 1
        fi
        local notes
        notes="$(cat "$notes_file")"
        jq --arg notes "$notes" '.notes = $notes' "$meta" > "$meta.new"
        mv "$meta.new" "$meta"
    fi
    if [ -n "$draft_flag" ]; then
        jq --arg draft "$draft_flag" '.draft = ($draft == "true")' "$meta" > "$meta.new"
        mv "$meta.new" "$meta"
    fi
    echo "fake-gh: edited release $tag"
}

cmd_upload() {
    local tag="$1"
    shift
    require_existing_release "$tag" upload

    local clobber="false"
    local -a asset_paths=()
    while [ $# -gt 0 ]; do
        case "$1" in
            --clobber) clobber="true"; shift ;;
            --*)
                echo "fake-gh: error: unsupported flag '$1' for 'release upload'" >&2
                exit 2 ;;
            *) asset_paths+=("$1"); shift ;;
        esac
    done

    local rel_dir
    rel_dir="$(release_dir_for_tag "$tag")"
    local meta="$rel_dir/metadata.json"
    local asset
    for asset in "${asset_paths[@]}"; do
        if [ ! -f "$asset" ]; then
            echo "fake-gh: error: asset '$asset' missing" >&2
            exit 1
        fi
        local name
        name="$(basename "$asset")"
        local target="$rel_dir/assets/$name"
        if [ -e "$target" ] && [ "$clobber" != "true" ]; then
            echo "fake-gh: error: asset '$name' already exists; use --clobber to replace" >&2
            exit 1
        fi
        cp "$asset" "$target"
        local sha
        sha="$(asset_sha256 "$asset")"
        # Replace or append in metadata.assets
        jq --arg name "$name" --arg sha "$sha" \
           '.assets = (.assets | map(select(.name != $name)) + [{"name": $name, "sha256": $sha}])' \
           "$meta" > "$meta.new"
        mv "$meta.new" "$meta"
    done
    echo "fake-gh: uploaded ${#asset_paths[@]} asset(s) to release $tag"
}

case "$sub" in
    view) cmd_view "$@" ;;
    create) cmd_create "$@" ;;
    edit) cmd_edit "$@" ;;
    upload) cmd_upload "$@" ;;
    *)
        echo "fake-gh: error: unsupported 'gh release $sub' (only view/create/edit/upload are faked)" >&2
        exit 2 ;;
esac
