# Server-side translation loading/saving

The browser cannot read or write `/root/mmorpg/server_lang` directly. This project includes a Node.js server that serves both:

- the built Vite frontend from `dist/`
- the API for reading/writing translation files from `TRANSLATION_ROOT`

## Fast start on a remote server

From the project directory:

```bash
chmod +x start-web-editor.sh
TRANSLATION_ROOT=/root/mmorpg/server_lang ./start-web-editor.sh start
```

The script will:

1. install npm dependencies if `node_modules` is missing;
2. build the frontend if `dist` is missing;
3. start the Node server in the background;
4. bind to `0.0.0.0`, so the editor is reachable from another machine;
5. use port `3001`, or the next free port if `3001` is already busy.

Useful commands:

```bash
./start-web-editor.sh status
./start-web-editor.sh logs
./start-web-editor.sh restart
./start-web-editor.sh stop
```

Custom port:

```bash
PORT=3010 TRANSLATION_ROOT=/root/mmorpg/server_lang ./start-web-editor.sh start
```

Custom bind host:

```bash
HOST=0.0.0.0 PORT=3010 TRANSLATION_ROOT=/root/mmorpg/server_lang ./start-web-editor.sh start
```

Open in browser:

```txt
http://YOUR_SERVER_IP:3001
```

If the script selected another port because `3001` was busy, use the port printed by the script.

## Manual production start

```bash
npm install
npm run build
HOST=0.0.0.0 PORT=3001 TRANSLATION_ROOT=/root/mmorpg/server_lang npm run start
```

Do not use `npm run preview` for this workflow. `vite preview` does not provide `/api/translations`.

## Fix for `EADDRINUSE :::3001`

`EADDRINUSE` means another process already listens on port `3001`.

Use one of these options:

```bash
PORT=3002 TRANSLATION_ROOT=/root/mmorpg/server_lang npm run start
```

or use the fast-start script, which automatically selects the next free port:

```bash
TRANSLATION_ROOT=/root/mmorpg/server_lang ./start-web-editor.sh start
```

To find the old process:

```bash
ss -ltnp | grep ':3001'
```

Then stop it if needed.

## Firewall / remote access

The server binds to `0.0.0.0` by default. If you cannot open the editor remotely, open the selected port in your firewall or hosting panel.

Examples:

```bash
ufw allow 3001/tcp
```

or, if the script selected `3002`:

```bash
ufw allow 3002/tcp
```

## API

- `GET /api/health` checks the server.
- `GET /api/translations` loads `index.json` and all listed `*.txt` files.
- `POST /api/translations/:file` saves one translation file, for example `ru.txt`.

The backend only allows safe file names: letters, digits, `_`, and `-`.
