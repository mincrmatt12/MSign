const path = require('path');
const { CleanWebpackPlugin } = require('clean-webpack-plugin');
const TerserJSPlugin = require("terser-webpack-plugin");
const MiniCssExtractPlugin  = require("mini-css-extract-plugin")
const CssMinimizerPlugin = require('css-minimizer-webpack-plugin');
const webpack = require('webpack');

let verstr = require('child_process')
	.execSync("git describe --tags --always")
	.toString();

module.exports = (env, options) => {
	const config = {
		entry: {
			page: ['whatwg-fetch',
				'./src/js/page.js',
				'./src/html/page.html']},
		output: {
			filename: '[name].js',
			path: path.resolve(__dirname, './web'),
			publicPath: '/'
		},
		module: {
			rules: [
				{
					test: /\.(sa|sc|c)ss$/,
					use: [
						MiniCssExtractPlugin.loader,
						'css-loader',
						'sass-loader'
					]
				},
				{
					test: /\.m?js$/,
					exclude: /(node_modules|bower_components)/,
					use: {
						loader: 'babel-loader',
						options: {
							presets: ['@babel/env', '@babel/react'],
							plugins: ['@babel/plugin-proposal-class-properties']
						}
					}
				},
				{
					test: /\.(html)$/,
					use: [{
						loader: 'file-loader',
						options: {
							name: '[name].[ext]'
						}
					}]
				}
			]
		},
		optimization: {
			minimize: options.mode == 'production',
			minimizer: [
				new TerserJSPlugin({
					terserOptions: {
						format: {
							comments: false,
						},
					},
					extractComments: false,
				}),
				new CssMinimizerPlugin()
			]
		},
		devtool: options.mode == 'development' ? 'eval-source-map' : false,
		plugins: [
			new MiniCssExtractPlugin({
				filename: "[name].css",
				chunkFilename: "[id].[chunkhash].css"
			}),
			new webpack.DefinePlugin({
				VERSION: JSON.stringify(verstr)
			})
		]
	}


	if (options.mode == 'development') {
		config.output.publicPath = 'http://localhost:8001/';
		config.devServer = {
			inline: true,
			port: 8001,
			contentBase: path.join(__dirname, './web'),
			stats: {color: true,},
			headers: {
				"Access-Control-Allow-Methods": "GET, POST, PUT, DELETE, PATCH, OPTIONS",
				"Access-Control-Allow-Headers": "X-Requested-With, content-type, Authorization",
				"Access-Control-Allow-Origin": "*"
			},
			historyApiFallback: {index: '/page.html'}
		}

	} else {
		config.plugins.push(new CleanWebpackPlugin({}));
	}

	return config;
};
