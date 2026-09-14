#!/bin/bash
set -e

cd "$(dirname "$0")"

translations=(translations/gxde-launcher.ts)
for ts in translations/gxde-launcher_*.ts; do
    case "$ts" in
        translations/gxde-launcher_ast.ts|translations/gxde-launcher_pam.ts)
            ;;
        *)
            translations+=("$ts")
            ;;
    esac
done

/usr/lib/qt6/bin/lupdate -recursive src/ -ts "${translations[@]}"

# Qt Linguist cannot update existing Asturian or Pampanga catalogs because its
# plural-rules table does not recognize their locale tags.  Use a compatible
# locale only while merging source messages, then restore the real TS language.
update_unsupported_locale() {
    local ts="$1"
    local language="$2"
    local compatible_language="$3"
    local temporary_ts

    temporary_ts=$(mktemp translations/.gxde-launcher-lupdate.XXXXXX.ts)
    /usr/lib/qt6/bin/lconvert -target-language "$compatible_language" \
        -i "$ts" -o "$temporary_ts"
    /usr/lib/qt6/bin/lupdate -recursive src/ -ts "$temporary_ts"
    /usr/lib/qt6/bin/lconvert -target-language "$language" \
        -i "$temporary_ts" -o "$ts"
    rm -f "$temporary_ts"
}

update_unsupported_locale translations/gxde-launcher_ast.ts ast_ES es_ES
update_unsupported_locale translations/gxde-launcher_pam.ts pam_PH fil_PH
