# Publishing OpenCyclo on Google Play

A step-by-step for getting `id.liostech.opencyclo` live, and for automating
uploads afterwards. The Play Console steps are manual: there is no API for
creating an app or completing its declarations.

## Before you start

**This is a personal developer account created after 13 November 2023, which
changes the route to production:**

| Track | Gate |
| --- | --- |
| Internal testing | None. Available as soon as the first release is uploaded. Up to 100 testers by email. |
| Closed testing | None to start, **but** production access requires a closed test with **at least 12 testers for 14 continuous days**, and Google checks that the testers genuinely used the app. |
| Production | Locked until the closed test completes and you apply for access. |

So the practical order is: internal → closed (12 testers, 14 days) → apply for
production. Closed testers must stay opted in for the whole period; if the count
drops below 12, the clock restarts.

**The package name is permanent.** It must be `id.liostech.opencyclo`, matching
what is already in the signed bundle.

### What already exists vs what you still need

| Item | Status |
| --- | --- |
| Signed AAB | `fastlane build` → `app/build/app/outputs/bundle/release/app-release.aab` |
| Store icon 512×512 | `app/android/fastlane/metadata/android/en-US/images/icon.png` — generated |
| Feature graphic 1024×500 | `.../images/featureGraphic.png` — generated placeholder (icon on black) |
| Title / short / full description | `.../title.txt`, `short_description.txt`, `full_description.txt` |
| Privacy policy text | `docs/privacy-policy.md` (needs a public URL) |
| **Phone screenshots** | **Missing — at least 2 required** |
| Target API level | API 36, which meets the requirement in force since 31 Aug 2026 |

## Step 1 — Create the app

Play Console → **All apps → Create app**.

- **App name:** `OpenCyclo: Bike Computer` (this is the store name and can differ
  from the installed launcher label, which is `OpenCyclo`)
- **Default language:** English (United States)
- **App or game:** App
- **Free or paid:** Free
- Accept the developer program policies and US export declarations.

The package name is attached on the first upload, not here.

## Step 2 — Declarations

Dashboard → **Set up your app**. Every one of these blocks the release, so fill
them in before uploading.

| Question | Answer for OpenCyclo |
| --- | --- |
| App access | **All functionality is available without special access.** There is no login. Use the free-text box to say that device features need the OpenCyclo hardware, and that the UI is otherwise fully browsable. |
| Ads | No ads |
| Content rating | Complete the IARC questionnaire: no violence, no sexuality, no language, no controlled substances, no gambling, no user-generated content, no digital purchases, no location **sharing with other users** (location goes to your own paired device). Expect Everyone / PEGI 3. |
| Target audience | **13+.** Do not select under-13, or the Families policy applies on top. |
| News app | No |
| Data safety | See the table below — this is the one to get right. |
| Government apps | No |
| Financial features | No |
| Health | No — this is a cycling computer companion, not a health or medical app. |
| Foreground service permissions | Required, because the app runs a `location` foreground service. Explain it keeps the phone's position flowing to the device while the screen is off. |
| Location permissions | Required, because the manifest requests `ACCESS_BACKGROUND_LOCATION`. Expect a slower review and possibly a request for a video showing the feature in use. |

### Data safety answers

The app has no accounts and no analytics, and ride data never leaves the phone
and the device. But it **does** send your AssistNow credentials and the GPS
module's identity to u-blox when you trigger a GPS sync, and that has to be
declared. See `docs/privacy-policy.md` for the detail.

| Data type | Collected / shared | Detail |
| --- | --- | --- |
| **Device or other IDs** | **Collected, shared** | The receiver identity messages (which include the GPS module's unique ID) and the Chipcode are sent to u-blox. Purpose: **App functionality**. Encrypted in transit: **yes**. Optional: **yes** — it happens only when the user taps to sync assisted GPS. |
| Location | **Not collected** | Read on the phone and sent to the paired device over Bluetooth; never transmitted off the device to a server. If Play review queries it, you can conservatively declare it as collected / not shared with purpose App functionality — it will not hurt approval. |
| Files and docs, Photos | Not collected | Read or written on the device only, at the user's request. |
| Health and fitness | Not collected | Heart rate and cadence come from your own BLE sensors and are written to local files; nothing is transmitted to us or to a third party. |
| Personal info, Financial info, Messages, App activity, Web browsing | Not collected | The app has no accounts, no analytics and no tracking. |

## Step 3 — Store listing

**Store listing** page. Paste the copy from the repo (single source of truth):

| Field | Value |
| --- | --- |
| App name | `.../metadata/android/en-US/title.txt` — 25 chars (limit 30) |
| Short description | `.../short_description.txt` — 55 chars (limit 80) |
| Full description | `.../full_description.txt` — 1710 chars (limit 4000) |
| App icon | `.../images/icon.png` — 512×512, no alpha |
| Feature graphic | `.../images/featureGraphic.png` — 1024×500, no alpha |
| Phone screenshots | **You must take these.** At least 2, up to 8 per type. PNG or JPEG, between 16:9 and 9:16, shortest side ≥ 320px, longest ≤ 3840px. Save as `.../images/phoneScreenshots/1.png`, `2.png`, … |
| App category | Sports (alternatives: Maps & Navigation, Health & Fitness) |
| Tags | cycling, bike computer, GPS |
| Contact email | Required — use an address you monitor |
| Privacy policy URL | Required — see below |

### Hosting the privacy policy

Play requires a public URL that needs no login. The policy lives at
`docs/privacy-policy.md`. The cheapest route is GitHub Pages on this repo,
serving the `/docs` folder, which renders that file at:

```
https://ibrahimhnif.github.io/opencyclo/privacy-policy.html
```

Enable it under Settings → Pages (source: `main`, folder `/docs`). Any other
static host works too.

Before publishing, confirm the contact address in the policy
(`privacy@liostech.id`) is one you actually receive mail at.

## Step 4 — First release

1. **Build the bundle:** `cd app/android && fastlane build`.
2. Play Console → **Testing → Internal testing → Create new release**, upload
   `app-release.aab`, add release notes, roll out.
3. When prompted, **enrol in Play App Signing**. Google then holds the app
   signing key and your `upload-keystore.p12` becomes the upload key — which is
   also what makes it possible to reset a lost upload key.
4. Add your own email as an internal tester, install from the opt-in link, and
   sanity-check the store listing text.
5. Create a **Closed testing** release (same bundle) and recruit ≥ 12 testers.
   Keep them opted in for 14 continuous days.
6. After that, **Apply for production access**, then promote the release to
   production. From then on, promote existing releases between tracks rather
   than uploading the same bundle again.

## Step 5 — Automating uploads (service account)

Do this after the first manual upload; the app must exist first.

1. In **Google Cloud Console**, create (or pick) a project.
2. Enable the **Google Play Android Developer API** for it.
3. Create a **service account** (no project-level role needed).
4. Create a **JSON key** for that service account and download it.
5. In **Play Console → Users and permissions → Invite new users**, paste the
   service account address (`...@....iam.gserviceaccount.com`) and grant it, for
   the OpenCyclo app: **Release to testing tracks**, **Manage testing tracks**,
   and **View app information**.
6. Store the key as a repository secret:

```sh
gh secret set PLAY_STORE_JSON_KEY < ~/Downloads/your-service-account-key.json
```

## Step 6 — Uploading from CI

`.github/workflows/play-upload.yml` runs on **manual dispatch only** — publishing
to a store should never be a side effect of a push. It reuses the release
signing secrets plus `PLAY_STORE_JSON_KEY`, builds the bundle and runs the
existing `fastlane internal` lane, which uploads to the internal testing track.

Actions → **Play upload** → Run workflow. The release lane's version check does
not apply here; the AAB's `versionCode` (from `pubspec.yaml`) just has to be
higher than the last one you uploaded.

## Gotchas

- **Bump `version:` in `app/pubspec.yaml` before every upload.** Play rejects a
  `versionCode` it has already seen.
- **Screenshots and the feature graphic are the only missing assets.** The
  feature graphic is a generated placeholder: the app icon centred on black. It
  is valid, but consider replacing it with branded artwork that carries the name.
- **Background location is the riskiest declaration.** Have a short screen
  recording ready showing the app feeding position to the device with the phone
  locked.
- **The 14-day clock is continuous.** Losing testers mid-period restarts it.
- **`INTERNET` is declared** in the manifest because Flutter's debug tooling
  needs it; the only network code in the app is the u-blox assist path described
  in the privacy policy.
