use anyhow::{anyhow, bail, Result};
use std::path::PathBuf;

pub fn get_juliaup_home_path() -> Result<PathBuf> {
    let entry_sep = if std::env::consts::OS == "windows" {';'} else {':'};

    let path = match std::env::var("JULIA_DEPOT_PATH") {
        Ok(val) => {
            let path = PathBuf::from(val.to_string().split(entry_sep).next().unwrap()); // We can unwrap here because even when we split an empty string we should get a first element.

            if !path.is_absolute() {
                bail!("The `JULIA_DEPOT_PATH` environment variable contains a value that resolves to an an invalid path `{}`.", path.display());
            };

            path
        }
        Err(_) => {
            let path = dirs::home_dir()
                .ok_or(anyhow!(
                    "Could not determine the path of the user home directory."
                ))?
                .join(".julia")
                .join("juliaup");

                if !path.is_absolute() {
                    bail!(
                        "The system returned an invalid home directory path `{}`.",
                        path.display()
                    );
                };

                path
        }
    };

    Ok(path)
}

pub fn get_juliaupconfig_path() -> Result<PathBuf> {
    let path = get_juliaup_home_path()?.join("juliaup.json");

    Ok(path)
}

pub fn get_arch() -> Result<String> {
    if std::env::consts::ARCH == "x86" {
        return Ok("x86".to_string());
    } else if std::env::consts::ARCH == "x86_64" {
        return Ok("x64".to_string());
    }

    bail!("Running on an unknown arch: {}.", std::env::consts::ARCH)
}

