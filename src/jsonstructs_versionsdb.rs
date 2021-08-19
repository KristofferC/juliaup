use serde::{Serialize, Deserialize};
use std::collections::HashMap;
use semver::Version;

#[derive(Serialize, Deserialize)]
pub struct JuliaupVersionDBVersion {
    #[serde(rename = "Url")]
    pub url: String
}

#[derive(Serialize, Deserialize)]
pub struct JuliaupVersionDBChannel {
    #[serde(rename = "Version")]
    pub version: Version
}

#[derive(Serialize, Deserialize)]
pub struct JuliaupVersionDB {
    #[serde(rename = "AvailableVersions")]
    pub available_versions: HashMap<Version,JuliaupVersionDBVersion>,
    #[serde(rename = "AvailableChannels")]
    pub available_channels: HashMap<String,JuliaupVersionDBChannel>
}
