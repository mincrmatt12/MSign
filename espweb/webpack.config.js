const path = require('path');
const CleanWebpackPlugin = require('clean-webpack-plugin');
const TerserJSPlugin = require("terser-webpack-plugin");
const MiniCssExtractPlugin  = require("mini-css-extract-plugin")
const OptimizeCSSAssetsPlugin = require("optimize-css-assets-webpack-plugin");
const webpack = require('webpack');

module.exports = (env, options) => {
	const config = {
		entry: {
			page: ['react-hot-loader/patch',
				'whatwg-fetch',
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
							presets: ['@babel/preset-env', '@babel/preset-react'],
							plugins: [
								'react-hot-loader/babel',
								["@babel/plugin-proposal-decorators", {"legacy": true}],
								["@babel/plugin-proposal-class-properties", {"loose": true}]
							]
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
			minimizer: [
				new TerserJSPlugin({}),
				new OptimizeCSSAssetsPlugin({})
			]
		}
	}

	config.plugins = [
		new MiniCssExtractPlugin({
			filename: "[name].css",
			chunkFilename: "[id].[chunkhash].css"
		}),
	]

	if (options.mode == 'development') {
		config.output.publicPath = 'http://localhost:8001/';
		config.plugins.push(new webpack.HotModuleReplacementPlugin());
		config.devtool = 'inline-source-map';
		config.devServer = {
			hot: true,
			inline: true,
			port: 8001,
			contentBase: path.join(__dirname, './web'),
			stats: {color: true,},
			headers: {
				"Access-Control-Allow-Methods": "GET, POST, PUT, DELETE, PATCH, OPTIONS",
				"Access-Control-Allow-Headers": "X-Requested-With, content-type, Authorization",
				"Access-Control-Allow-Origin": "*"
			}
		}

	} else {
		config.plugins.push(new CleanWebpackPlugin());
	}

	return config;
};
