<?php
use Illuminate\Database\Migrations\Migration;use Illuminate\Database\Schema\Blueprint;use Illuminate\Support\Facades\Schema;
return new class extends Migration {
 public function up():void {
  Schema::table('users',function(Blueprint $t){$t->string('discord_id')->nullable()->unique();$t->string('signin_method')->default('email');$t->boolean('disabled')->default(false);});
  Schema::create('email_codes',function(Blueprint $t){$t->string('email')->primary();$t->string('digest');$t->unsignedInteger('attempts')->default(0);$t->unsignedBigInteger('expires');});
 }
 public function down():void {Schema::dropIfExists('email_codes');Schema::table('users',function(Blueprint $t){$t->dropColumn(['discord_id','signin_method','disabled']);});}
};
