/** Provider-configured web auth. Not mounted on the unauthenticated preview. */
import {betterAuth} from 'better-auth';
import {emailOTP} from 'better-auth/plugins';
import {getMigrations} from 'better-auth/db/migration';
export function createWebAuth({database,baseURL,secret,discord,sendEmailCode}={}){
 const url=new URL(baseURL);
 if(url.protocol!=='https:'||url.username||url.password||url.pathname!=='/'||url.search||url.hash)throw Error('Auth requires a canonical HTTPS origin');
 if(typeof secret!=='string'||secret.length<32)throw Error('Auth requires a secret of at least 32 characters');
 if(!database)throw Error('A separate auth database is required');
 if(discord&&(!discord.clientId||!discord.clientSecret))throw Error('Discord credentials are incomplete');
 if(!discord&&typeof sendEmailCode!=='function')throw Error('Configure at least one real sign-in provider');
 const options={appName:'Pixel Doodle',baseURL:url.origin,basePath:'/api/auth',secret,database,
  trustedOrigins:[url.origin],emailAndPassword:{enabled:false},
  session:{expiresIn:7*86400,updateAge:86400,cookieCache:{enabled:false}},
  account:{accountLinking:{enabled:false,disableImplicitLinking:true},encryptOAuthTokens:true},
  advanced:{useSecureCookies:true,cookiePrefix:'pixel-doodle',defaultCookieAttributes:{httpOnly:true,secure:true,sameSite:'lax'}},
  rateLimit:{enabled:true,storage:'database',window:60,max:30},
  socialProviders:discord?{discord:{clientId:discord.clientId,clientSecret:discord.clientSecret,disableDefaultScope:true,scope:['identify','email']}}:{},
  plugins:typeof sendEmailCode==='function'?[emailOTP({otpLength:6,expiresIn:300,allowedAttempts:3,storeOTP:'hashed',resendStrategy:'rotate',sendVerificationOTP:async({email,otp,type})=>{if(type!=='sign-in')throw Error('This email operation is not enabled');await sendEmailCode({email,otp});}})]:[],
  logger:{disabled:true}
 };
 const auth=betterAuth(options);
 return {auth,options,async migrate(){const migration=await getMigrations(options);await migration.runMigrations();}};
}
