#!/bin/sh

set -eu

GITHUB_API_URL=https://api.github.com

GITHUB_TOKEN=$1

curl -o release.json \
    --header "Authorization: token ${GITHUB_TOKEN}" \
    --request GET \
    ${GITHUB_API_URL}/repos/${GITHUB_REPOSITORY}/releases/${GITHUB_REF#"refs/"}

UPLOAD_URL=$(jq -r .upload_url release.json)

DOWNLOAD_LIST=$(jq -r ".assets | .[] | .url" release.json)

rm release.json

mkdir dist

echo "$DOWNLOAD_LIST" | while IFS= read -r line 
do 
	curl -L -o $(basename $line).zip --header "Authorization: token ${GITHUB_TOKEN}" --header "Accept: application/octet-stream" --request GET $line
	unzip -o $(basename $line).zip -d dist
	rm $(basename $line).zip
done

PLUGIN_BUILD_SLUG=$(jq -r .slug plugin.json)
PLUGIN_BUILD_NAME=${PLUGIN_BUILD_SLUG}-$(jq -r .version plugin.json)
cd dist 
zip -q -9 -r ${PLUGIN_BUILD_NAME}.zip ./${PLUGIN_BUILD_SLUG}
cd ..

ASSET_PATH=$(ls dist/*.zip)

curl -i \
    --header "Authorization: token ${GITHUB_TOKEN}" \
    --header "Content-Type: application/zip" \
    --request POST \
    --data-binary @"${ASSET_PATH}" \
    ${UPLOAD_URL%"{?name,label\}"}?name=${ASSET_PATH#"dist/"}
