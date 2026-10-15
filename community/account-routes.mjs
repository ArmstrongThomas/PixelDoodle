/** HTTP boundary for configured auth and paired-device account actions. */
export function accountRoutes({auth,origin,providers,pairing}={}){
 return async(req,res)=>{
  const path=new URL(req.url,'http://local').pathname;
  if(!path.startsWith('/api/auth/')&&!path.startsWith('/api/account/'))return false;
  const send=(status,data)=>{res.writeHead(status,{'Content-Type':'application/json','Cache-Control':'no-store','X-Content-Type-Options':'nosniff'});res.end(JSON.stringify(data));};
  if(path==='/api/account/status'&&req.method==='GET'&&!auth){send(200,{enabled:false,providers:[]});return true;}
  if(!auth){send(503,{error:'Sign-in is not configured yet.'});return true;}
  try{
   if(req.headers.host!==new URL(origin).host){send(403,{error:'Use the configured sign-in address.'});return true;}
   if(!['GET','POST'].includes(req.method)){send(405,{error:'Method not allowed'});return true;}
   if(req.method==='POST'&&req.headers.origin!==origin){send(403,{error:'Origin rejected'});return true;}
   let bytes=Buffer.alloc(0);
   if(req.method==='POST'){
    if(!/^application\/json(?:;|$)/i.test(req.headers['content-type']||'')){send(415,{error:'JSON required'});return true;}
    for await(const part of req){if(bytes.length+part.length>8192){send(413,{error:'Request too large'});return true;}bytes=Buffer.concat([bytes,part]);}
   }
   const headers=new Headers();for(const [key,value] of Object.entries(req.headers))if(value!==undefined)headers.set(key,Array.isArray(value)?value.join(','):value);
   // Never accept caller-provided proxy addresses as authentication rate subjects.
   headers.delete('x-forwarded-for');headers.delete('x-real-ip');headers.delete('cf-connecting-ip');headers.set('x-real-ip',req.socket.remoteAddress||'unknown');
   if(path.startsWith('/api/auth/')){
    const allowed={'/api/auth/get-session':'GET','/api/auth/callback/discord':'GET','/api/auth/sign-in/social':'POST','/api/auth/email-otp/send-verification-otp':'POST','/api/auth/sign-in/email-otp':'POST','/api/auth/sign-out':'POST'};
    if(allowed[path]!==req.method){send(404,{error:'Not found'});return true;}
    const response=await auth.handler(new Request(origin+req.url,{method:req.method,headers,...(req.method==='POST'?{body:bytes}:{} )}));
    res.statusCode=response.status;for(const [key,value] of response.headers)if(key!=='set-cookie')res.setHeader(key,value);
    const cookies=response.headers.getSetCookie();if(cookies.length)res.setHeader('Set-Cookie',cookies);res.setHeader('Cache-Control','no-store');res.end(Buffer.from(await response.arrayBuffer()));return true;
   }
   const session=await auth.api.getSession({headers});
   if(path==='/api/account/status'&&req.method==='GET'){send(200,{enabled:true,providers,user:session?{id:session.user.id,name:session.user.name,email:session.user.email}:null});return true;}
   if(!session||!session.user.emailVerified){send(401,{error:'Sign in with a verified account first.'});return true;}
   const id=session.user.id;
   if(path==='/api/account/devices'&&req.method==='GET'){send(200,{devices:pairing.devices(id)});return true;}
   if(req.method!=='POST'){send(405,{error:'POST required'});return true;}
   let body;try{body=JSON.parse(bytes.toString('utf8'));}catch{send(400,{error:'Invalid JSON'});return true;}
   if(!body||typeof body!=='object'||Array.isArray(body)){send(400,{error:'Invalid JSON object'});return true;}
   if(path==='/api/account/pair/inspect'){send(200,pairing.inspect(id,body.code));return true;}
   if(path==='/api/account/pair/decide'){send(200,pairing.decide(id,body.code,body.approve));return true;}
   if(path==='/api/account/devices/revoke'){if(typeof body.id!=='string'||body.id.length>64){send(400,{error:'Invalid device'});return true;}send(200,{revoked:pairing.revoke(id,body.id)});return true;}
   send(404,{error:'Not found'});
  }catch(e){const known=['unauthorized','rate_limited','invalid_or_expired_code','explicit_decision_required'];send(e.code==='rate_limited'?429:known.includes(e.code)?400:500,{error:known.includes(e.code)?e.code:'Account request failed'});}
  return true;
 };
}
