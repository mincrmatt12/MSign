#ifndef UPD_H
#define UPD_H

namespace upd {
	enum UpdateKind {
		FULL_SYSTEM = 0x01,
		WEB_UI,
		CERTIFICATE_FILE,

		NO_UPDATE = 0x00
	};

	UpdateKind needed(); // is an update needed, based on the state of the SD card
	
	void update_cacerts();
	void update_website();
}

#endif /* ifndef UPD_H */
