# Android release and deployment

How a build of the companion app gets from `app/` to a downloadable, signed
artifact. iOS is out of scope: it needs an Apple Developer account and a
registered App ID.

## Artifacts

| Artifact | Built by | Path |
| --- | --- | --- |
| Signed bundle (Play) | `flutter build appbundle --release` | `app/build/app/outputs/bundle/release/app-release.aab` |
| Signed APK (sideload) | `flutter build apk --release` | `app/build/app/outputs/flutter-apk/app-release.apk` |

Both are signed with the upload keystore when `app/android/key.properties`
exists. **Without it the release build silently falls back to the debug key** —
that is deliberate so a fresh clone can still run `flutter build apk --release`,
but a debug-signed artifact must never be shipped or published.

## Signing

`app/android/upload-keystore.p12` (PKCS12, alias `upload`, valid until 2054)
plus `app/android/key.properties` hold the credentials. Both are gitignored;
`key.properties.example` documents the format.

```
storePassword=<password>
keyPassword=<password>
keyAlias=upload
storeFile=upload-keystore.p12
```

`storeFile` resolves against `app/android/`, so a bare filename points at the
file next to `key.properties`.

> **Back the keystore up somewhere you will not lose it.** Without Play App
> Signing, a lost upload key means you cannot ship an update to an existing
> listing without a key reset. Store it in a password manager, not in the repo.

To replace it:

```sh
cd app/android
keytool -genkeypair -v -keystore upload-keystore.p12 -storetype PKCS12 \
  -keyalg RSA -keysize 2048 -validity 10000 -alias upload
```

## Building locally

Fastlane is installed globally, like the other CLIs this repo uses (`flutter`,
`pio`, `gh`) — there is no Gemfile, so CI and a local shell run the same
command.

```sh
gem install fastlane      # once; or: brew install fastlane
cd app/android
fastlane build
```

`fastlane build` runs both Flutter release builds and fails if either artifact
is missing. The version comes from `pubspec.yaml` (`version: 1.0.0+1` →
`versionName` `1.0.0`, `versionCode` `1`); Fastlane does not bump it.

## Publishing a GitHub Release

1. Bump `version:` in `app/pubspec.yaml`.
2. Tag the same version and push:

```sh
git tag v1.0.0
git push origin v1.0.0
```

The `Android release` workflow (`.github/workflows/android-release.yml`) then
decodes the keystore from secrets, runs `fastlane release`, and creates a GitHub
Release for the tag with the `.aab` and `.apk` attached. The lane refuses to run
if the tag does not match the pubspec version.

Required repository secrets (Settings → Secrets and variables → Actions):

| Secret | Value |
| --- | --- |
| `ANDROID_KEYSTORE_BASE64` | `base64 -i app/android/upload-keystore.p12 \| pbcopy` |
| `ANDROID_KEYSTORE_PASSWORD` | `storePassword` from `key.properties` |
| `ANDROID_KEY_ALIAS` | `upload` |
| `ANDROID_KEY_PASSWORD` | `keyPassword` from `key.properties` |

`workflow_dispatch` is also enabled, but it defaults to `github.ref_name`, so
dispatch it from a tag or the version check will fail.

## Google Play

The app exists in the Play Console as `id.liostech.opencyclo`, with the store
listing drafted from `app/android/fastlane/metadata/android/en-US/`. See
`docs/play-console-setup.md` for the console steps, the declarations and their
answers, and the production-access gate.

Uploading to the internal testing track needs a Play service-account JSON with
release permissions on the app. Once that exists:

```sh
cd app/android
PLAY_STORE_JSON_KEY_PATH=~/play-service-account.json fastlane internal
```

From CI, run the **Play upload** workflow (`workflow_dispatch`) after adding the
key as the `PLAY_STORE_JSON_KEY` repository secret; it builds the bundle itself.
The `internal` lane fails with a clear message when the JSON key is missing.
