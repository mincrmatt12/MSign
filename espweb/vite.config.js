import {defineConfig} from 'vite'
import react from '@vitejs/plugin-react'
import {resolve} from 'path'
import {execSync} from 'child_process';

export default (() => {
	let verstr = execSync("git describe --tags --always").toString();
	process.env.VITE_MSIGN_GIT_VER = verstr;

	return defineConfig({
		root: "./src/",
		plugins: [
			react(),
			{
				name: "manual-spa",
				configureServer(server) {
					server.middlewares.use(
						(req, _, next) => {
							if (req.url == "/" || (req.url.match(/^\/\w+\/?$/))) req.url = "/page.html";
							next();
						}
					)
				}
			}
		],
		build: {
			outDir: "../web/",
			emptyOutDir: true,
			minify: "terser",
			chunkSizeWarningLimit: 1000,
			rollupOptions: {
				input: {
					page: resolve(__dirname, "./src/page.html")
				},
				output: {
					manualChunks: (id) => {
						if (id.includes("node_modules")) return "vendor";
						else return "page";
					},
					chunkFileNames: '[name].[hash:16].js',
					assetFileNames: '[name].[hash:16].[ext]',
					entryFileNames: '[name].[hash:16].js'
				}
			}
		},
		appType: "mpa"
	});
});
