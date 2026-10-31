const path = require('path');
const HtmlWebpackPlugin = require('html-webpack-plugin');
const MiniCssExtractPlugin = require('mini-css-extract-plugin');

const isProduction = process.env.NODE_ENV === 'production';

module.exports = {
  mode: isProduction ? 'production' : 'development',
  devtool: isProduction ? 'source-map' : 'cheap-module-source-map',
  entry: {
    app: './src/ui/AdapterWeb.tsx',
  },
  output: {
    path: path.resolve(__dirname, 'dist/web'),
    filename: isProduction ? '[name].[contenthash].js' : '[name].js',
    chunkFilename: isProduction ? '[name].[contenthash].chunk.js' : '[name].chunk.js',
    publicPath: '/',
    clean: true,
  },

  devServer: {
    port: 3000,
    host: '0.0.0.0',
    hot: true,
    compress: true,
    historyApiFallback: true,
    static: {
      directory: path.join(__dirname, 'public'),
    },
    headers: {
      'Access-Control-Allow-Origin': '*',
    },
  },

  optimization: {
    minimize: isProduction,
    runtimeChunk: 'single',
    splitChunks: {
      chunks: 'all',
      cacheGroups: {
        vendor: {
          test: /[\\/]node_modules[\\/]/,
          name: 'vendors',
          priority: 10,
          reuseExistingChunk: true,
        },
        react: {
          test: /[\\/]node_modules[\\/](react|react-dom|react-dom\/client)[\\/]/,
          name: 'react-vendors',
          priority: 20,
        },
        common: {
          minChunks: 2,
          priority: 5,
          reuseExistingChunk: true,
        },
      },
    },
  },

  module: {
    rules: [
      {
        test: /\.tsx?$/,
        use: {
          loader: 'ts-loader',
          options: {
            transpileOnly: !isProduction,
            experimentalWatchApi: true,
          },
        },
        exclude: /node_modules/,
      },
      {
        test: /\.css$/i,
        use: [
          isProduction ? MiniCssExtractPlugin.loader : 'style-loader',
          'css-loader',
          'postcss-loader',
        ],
      },
      {
        test: /\.(png|svg|jpg|jpeg|gif)$/i,
        type: 'asset/resource',
      },
      {
        test: /\.(woff|woff2|eot|ttf|otf)$/i,
        type: 'asset/resource',
      },
    ],
  },

  resolve: {
    extensions: ['.tsx', '.ts', '.js', '.json'],
    alias: {
      '@': path.resolve(__dirname, 'src/'),
    },
  },

  plugins: [
    new HtmlWebpackPlugin({
      template: path.resolve(__dirname, 'public/app.html'),
      filename: 'index.html',
      minify: isProduction && {
        removeComments: true,
        collapseWhitespace: true,
        removeAttributeQuotes: true,
      },
    }),
    new MiniCssExtractPlugin({
      filename: isProduction ? '[name].[contenthash].css' : '[name].css',
      chunkFilename: isProduction ? '[id].[contenthash].css' : '[id].css',
    }),
  ],

  performance: {
    maxEntrypointSize: 512000,
    maxAssetSize: 512000,
    hints: isProduction ? 'warning' : false,
  },

  target: 'web',
};
