# Preparing a release

Run the **Prepare release** GitHub Actions workflow manually to build a release
draft. Review and publish the draft from the Releases page.

## One-time setup

Push `.github/workflows/prepare-release.yml` and its supporting files to the
repository's default branch. Enable GitHub Actions in repository settings if needed.
The workflow uses the built-in `GITHUB_TOKEN` with `contents: write` for the release
job; no personal token is required. Repository or organization policies must allow
that permission and the checkout action. The workflow must exist on the default
branch for the manual Run workflow button to appear.

## Prepare the version

1. Finish and commit feature changes separately. Test the candidate in game,
   including normal treasure detection, menu/minimap gating and sound controls.
2. Set the same mod version in `CMakeLists.txt`, `README.md`, `CHANGELOG.md` and
   `release_notes.md`. Move ready changes out of Unreleased into the version's
   changelog section. Release notes must begin with `# Pirate Hat HUD X.Y.Z`.
3. Run formatting checks and `./cmake/Build.ps1`. Record actual validation and
   game compatibility in the notes; do not describe pending checks as passed.
4. Commit the release files with the exact subject `Release X.Y.Z` and the
   Changes, Validation and Compatibility body labels defined in `AGENTS.md`.
5. Create an annotated `vX.Y.Z` tag on that release commit and push the commit
   and that specific tag to GitHub. Replace the example version below:

```powershell
git tag -a v0.6.0 -m "Release 0.6.0"
git push origin main
git push origin v0.6.0
```

Do not move an existing release tag. The tag must contain the build script,
metadata validator and tests used by the workflow; historical tags created before
this automation was added cannot be prepared with this workflow.

## Build the draft

Open **Actions > Prepare release > Run workflow**. Select the default branch
and enter the existing tag, for example `v0.6.0`.

The workflow checks out that tag, validates release metadata and commit intent,
checks formatting, builds Release on Windows, runs architecture-check, CTest and
research-tool tests, and packages the mod. Only after those steps pass does it
create a draft named `Pirate Hat HUD X.Y.Z`, attach `PirateHatHUD-X.Y.Z.zip` and
copy the body from the tagged `release_notes.md`.

The ZIP includes the ASI, INI, package metadata and third-party notices/licenses.
It is the installable asset; GitHub's automatic source archives are not mod packages.

## Publish

Review the draft's notes and ZIP in **Releases**, then publish it manually when
ready. Upload that same ZIP to Nexus if desired; Nexus publication is separate.

A failed build creates no release. A failed upload may leave a partial draft;
inspect Releases before retrying. Repeated runs for the same tag are serialized,
and an existing draft or published release is not overwritten. To retry a partial
draft, remove that unpublished draft after review, keeping the tag, then rerun.
If the code or version needs correction, prepare a new release commit/tag instead.
