use std::{env, fs};

use zed_extension_api::{self as zed, LanguageServerId, Worktree};

const SERVER_PACKAGE: &str = "beast-language-server";
const SERVER_VERSION: &str = "0.2.1";
const SERVER_SCRIPT: &str = "node_modules/beast-language-server/dist/server.js";

struct BeastExtension {
    server_ready: bool,
}

impl BeastExtension {
    fn server_exists(&self) -> bool {
        fs::metadata(SERVER_SCRIPT).is_ok_and(|metadata| metadata.is_file())
    }

    fn server_script(&mut self, language_server_id: &LanguageServerId) -> zed::Result<String> {
        if self.server_ready && self.server_exists() {
            return Ok(SERVER_SCRIPT.to_string());
        }

        zed::set_language_server_installation_status(
            language_server_id,
            &zed::LanguageServerInstallationStatus::CheckingForUpdate,
        );
        let installed_version = zed::npm_package_installed_version(SERVER_PACKAGE)?;
        if !self.server_exists() || installed_version.as_deref() != Some(SERVER_VERSION) {
            zed::set_language_server_installation_status(
                language_server_id,
                &zed::LanguageServerInstallationStatus::Downloading,
            );
            if let Err(error) = zed::npm_install_package(SERVER_PACKAGE, SERVER_VERSION) {
                if !self.server_exists() {
                    return Err(error);
                }
                eprintln!(
                    "failed to update {SERVER_PACKAGE} to {SERVER_VERSION}; using the existing installation: {error}"
                );
            }
        }

        if !self.server_exists() {
            return Err(format!(
                "installed {SERVER_PACKAGE}@{SERVER_VERSION}, but {SERVER_SCRIPT} was not found"
            ));
        }
        self.server_ready = true;
        Ok(SERVER_SCRIPT.to_string())
    }
}

impl zed::Extension for BeastExtension {
    fn new() -> Self {
        Self {
            server_ready: false,
        }
    }

    fn language_server_command(
        &mut self,
        language_server_id: &LanguageServerId,
        worktree: &Worktree,
    ) -> zed::Result<zed::Command> {
        if let Some(command) = worktree.which(SERVER_PACKAGE) {
            return Ok(zed::Command {
                command,
                args: vec!["--stdio".to_string()],
                env: Default::default(),
            });
        }

        if worktree.read_text_file(SERVER_SCRIPT).is_ok() {
            return Ok(zed::Command {
                command: zed::node_binary_path()?,
                args: vec![SERVER_SCRIPT.to_string(), "--stdio".to_string()],
                env: Default::default(),
            });
        }

        let server_script = env::current_dir()
            .map_err(|error| format!("failed to locate the extension directory: {error}"))?
            .join(self.server_script(language_server_id)?)
            .to_string_lossy()
            .to_string();
        Ok(zed::Command {
            command: zed::node_binary_path()?,
            args: vec![server_script, "--stdio".to_string()],
            env: Default::default(),
        })
    }
}

zed::register_extension!(BeastExtension);
