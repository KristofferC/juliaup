use crate::config_file::JuliaupConfig;
use crate::config_file::JuliaupConfigChannel;
use crate::config_file::JuliaupConfigVersion;
use crate::jsonstructs_versionsdb::JuliaupVersionDB;
use crate::utils::get_juliaup_home_path;
use anyhow::{anyhow, Context, Result};
use console::style;
use flate2::read::GzDecoder;
use indicatif::{ProgressBar, ProgressStyle};
use semver::Version;
use std::{
    io::Read,
    path::{Component::Normal, Path, PathBuf},
};
use tar::Archive;

fn unpack_sans_parent<R, P>(mut archive: Archive<R>, dst: P) -> Result<()>
where
    R: Read,
    P: AsRef<Path>,
{
    for entry in archive.entries()? {
        let mut entry = entry?;
        let path: PathBuf = entry
            .path()?
            .components()
            .skip(1) // strip top-level directory
            .filter(|c| matches!(c, Normal(_))) // prevent traversal attacks TODO We should actually abort if we come across a non-standard path element
            .collect();
        entry.unpack(dst.as_ref().join(path))?;
    }
    Ok(())
}

fn download_extract_sans_parent(url: &String, target_path: &Path) -> Result<()> {
    let response = ureq::get(url)
        .call()
        .with_context(|| format!("Failed to download from url `{}`.", url))?;

    let content_length = response.header("Content-Length").and_then(|v| v.parse::<u64>().ok() );

    let pb = match content_length {
        Some(content_length) => ProgressBar::new(content_length),
        None => ProgressBar::new_spinner(),
    };
    
    pb.set_prefix("  Downloading:");
    pb.set_style(ProgressStyle::default_bar()
    .template("{prefix:.cyan.bold} [{bar}] {bytes}/{total_bytes} eta: {eta}")
                .progress_chars("=> "));

    let foo = pb.wrap_read(response.into_reader());

    let tar = GzDecoder::new(foo);
    let archive = Archive::new(tar);
    unpack_sans_parent(archive, &target_path)
        .with_context(|| format!("Failed to extract downloaded file from url `{}`.", url))?;
    Ok(())
}

pub fn install_version(
    version: &Version,
    config_data: &mut JuliaupConfig,
    version_db: &JuliaupVersionDB,
) -> Result<()> {

    // Return immediately if the version is already installed.
    if config_data.installed_versions.contains_key(&version) {
        return Ok(());
    }

    let download_url = version_db
        .available_versions
        .get(version)
        .ok_or(anyhow!(
            "Failed to find download url in versions db for '{}'.",
            version
        ))?
        .url
        .clone();


    let child_target_foldername = format!("julia-{}", version);

    let target_path = get_juliaup_home_path()
        .with_context(|| "Failed to retrieve juliap folder while trying to install new version.")?
        .join(&child_target_foldername);

    std::fs::create_dir_all(target_path.parent().unwrap())?;

    eprintln!("{} Julia {}.", style("Installing").green().bold(), version);

    download_extract_sans_parent(&download_url, &target_path)?;

    let mut rel_path = PathBuf::new();
    rel_path.push(".");
    rel_path.push(&child_target_foldername);

    config_data.installed_versions.insert(
        version.clone(),
        JuliaupConfigVersion {
            path: rel_path,
        },
    );

    Ok(())
}

pub fn garbage_collect_versions(config_data: &mut JuliaupConfig) -> Result<()> {
    let home_path = get_juliaup_home_path().with_context(|| {
        "Failed to retrieve juliap folder while trying to garbage collect versions."
    })?;

    let mut versions_to_uninstall: Vec<Version> = Vec::new();
    for (installed_version, detail) in &config_data.installed_versions {
        if config_data.installed_channels.iter().all(|j| match &j.1 {
            JuliaupConfigChannel::SystemChannel { version } => version != installed_version,
            JuliaupConfigChannel::LinkedChannel {
                command: _,
                args: _,
            } => true,
        }) {
            let path_to_delete = home_path.join(&detail.path);
            let display = path_to_delete.display();

            match std::fs::remove_dir_all(&path_to_delete) {
            Err(_) => eprintln!("WARNING: Failed to delete {}. You can try to delete at a later point by running `juliaup gc`.", display),
            Ok(_) => ()
        };
            versions_to_uninstall.push(installed_version.clone());
        }
    }

    for i in versions_to_uninstall {
        config_data.installed_versions.remove(&i);
    }

    Ok(())
}
