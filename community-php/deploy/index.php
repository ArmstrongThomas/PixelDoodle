<?php
// Copy ONLY this file and .htaccess to F:/pixeldoodle.
// Application source, .env, vendor and storage stay outside Apache's webroot.
use Illuminate\Http\Request;
define('LARAVEL_START', microtime(true));
$appRoot = 'F:/pixeldoodle-app/community-php';
if (file_exists($maintenance = $appRoot.'/storage/framework/maintenance.php')) require $maintenance;
require $appRoot.'/vendor/autoload.php';
$app = require_once $appRoot.'/bootstrap/app.php';
$app->handleRequest(Request::capture());
