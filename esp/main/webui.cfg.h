namespace webui {
	enum struct WebuiMode {
		DISABLED,
		LOCAL,
		PROXIED
	};

	//!cfg: holds .webui.mode, default webui::WebuiMode::LOCAL
	extern WebuiMode mode;

	//!cfg: holds .webui.login.user, default "admin"
	extern const char * login_user;

	//!cfg: holds .webui.login.password, default "admin"
	extern const char * login_password;
}
