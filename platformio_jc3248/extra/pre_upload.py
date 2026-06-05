"""Warn if COM port busy before upload only."""
Import("env")

upload_targets = [str(t) for t in BUILD_TARGETS]
if "upload" not in upload_targets and "uploadfs" not in upload_targets:
    pass
else:
    try:
        import serial
        from serial.tools import list_ports
    except ImportError:
        pass
    else:
        def find_esp_ports():
            ports = []
            for p in list_ports.comports():
                hwid = (p.hwid or "").upper()
                if "239A" in hwid or "10C4" in hwid or "1A86" in hwid:
                    ports.append(p.device)
            return ports

        def try_open(port):
            try:
                s = serial.Serial(port, 115200, timeout=0.2)
                s.close()
                return True, None
            except Exception as e:
                return False, str(e)

        upload_port = env.subst("$UPLOAD_PORT")
        if not upload_port:
            ports = find_esp_ports()
            upload_port = ports[0] if ports else None

        if not upload_port:
            print("[pre_upload] No COM — plug board, HOLD BOOT, upload")
        else:
            ok, err = try_open(upload_port)
            if ok:
                print(f"[pre_upload] {upload_port} OK")
            else:
                print(f"[pre_upload] WARN: close Serial Monitor ({err})")
