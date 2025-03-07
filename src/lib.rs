pub mod utils;
pub mod jsonstructs_versionsdb;
pub mod config_file;
pub mod versions_file;
pub mod operations;
pub mod command_add;
pub mod command_default;
pub mod command_gc;
pub mod command_link;
pub mod command_status;
pub mod command_remove;
pub mod command_update;
pub mod command_initial_setup_from_launcher;

include!(concat!(env!("OUT_DIR"), "/bundled_version.rs"));

pub fn get_bundled_julia_full_version() -> &'static str {
    BUNDLED_JULIA_FULL_VERSION
}
