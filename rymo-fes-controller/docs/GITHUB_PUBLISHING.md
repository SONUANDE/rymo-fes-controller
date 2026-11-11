# Publish this project on GitHub

## Before publishing

The source report identifies RYMO Technologies Pvt. Ltd. and contains employee and supervisor names. The workbook contains project measurement data. Confirm that you have permission to publish the company project, firmware, data, report, and media. Keep the first repository private until that is clear. This project has no license; do not add one unless the rights holder approves it.

The complete bundle keeps original gallery files under `source-materials/`, and `.gitignore` excludes that directory. The GitHub-ready archive omits it. The tracked repository includes the extracted firmware listing, host plotter, load-analysis workbook and PDF, curated documentation, and optimized photos.

## Option A: GitHub Desktop

1. Extract `rymo-fes-controller-github.zip` to a folder you can find again.
2. Install and sign in to GitHub Desktop if needed.
3. Choose **File > Add Local Repository** and select the extracted `rymo-fes-controller` folder. If it is not already a Git repository, choose the option to create one.
4. Review the changed-files list. Confirm `source-materials/` is absent and the analysis workbook is present.
5. Enter a commit summary such as `Prepare FES project repository`, then commit to `main`.
6. Choose **Publish repository** and keep it private unless you have approval to make it public.
7. Open the repository page and check that the README, workbook, photos, firmware, logger, and docs are present.

## Option B: Git command line

Create an empty repository on GitHub first. Do not initialize it with a README, license, or `.gitignore`, since this folder already contains those files. Then open PowerShell or Git Bash in the extracted project folder and run:

```powershell
git init -b main
git status --short
git add .
git status --short
git commit -m "Prepare FES project repository"
git remote add origin https://github.com/<your-username>/rymo-fes-controller.git
git push -u origin main
```

Replace `<your-username>` with your GitHub username and use the exact remote URL shown by GitHub. Review `git status --short` before committing. `source-materials/` should not appear. The analysis workbook should appear.

The workbook is about 25.1 MiB. GitHub's website uploader is limited to 25 MiB per file, so push it using GitHub Desktop or Git rather than the web uploader.

## Optional: include approved demo videos with Git LFS

The original videos are excluded from Git by design. If you have permission to publish them and want them on GitHub:

1. Install Git LFS and run `git lfs install`.
2. Copy only the approved demo clips from `source-materials/original-gallery/` to a new `media/videos/` directory.
3. Run `git lfs track "media/videos/*.mp4"` from the repository root.
4. Confirm `.gitattributes` contains the MP4 rule, then add, commit, and push the approved files.

Git LFS uses separate storage and bandwidth allowances. Check your GitHub plan before uploading large videos. GitHub's current guidance: [large files](https://docs.github.com/en/repositories/working-with-files/managing-large-files/about-large-files-on-github), [Git LFS configuration](https://docs.github.com/en/repositories/working-with-files/managing-large-files/configuring-git-large-file-storage), and [adding existing code](https://docs.github.com/en/migrations/importing-source-code/using-the-command-line-to-import-source-code/adding-locally-hosted-code-to-github).

## Suggested repository details

- **Name:** `rymo-fes-controller`
- **Description:** `nRF52832 internship prototype for BLE-controlled, dual-channel FES output and ADC feedback`
- **Topics:** `nrf52832`, `embedded-c`, `ble`, `fes`, `embedded-systems`, `nrf5-sdk`
- **Visibility:** private until publication rights are confirmed
