/** Opt-in native pairing transport. Cookies never authorize these endpoints. */
export function deviceRoutes({origin,pairing,enableDevicePairing=false}={}){
 if(enableDevicePairing&&(!pairing||new URL(origin).protocol!=='https:'))throw Error('Pairing requires HTTPS origin and account-backed service');
 return async(req,res)=>{
  const url=new URL(req.url,'http://local');
  if(!url.pathname.startsWith('/api/device/'))return false;
  const send=(status,data)=>{res.writeHead(status,{'Content-Type':'application/json','Cache-Control':'no-store','X-Content-Type-Options':'nosniff','Referrer-Policy':'no-referrer',...(status>=400?{'Connection':'close'}:{})});res.end(JSON.stringify(data));};
  if(!enableDevicePairing){send(503,{error:'pairing_unavailable'});return true;}
  try{
   if(req.headers.host!==new URL(origin).host||(req.headers.origin!==undefined&&req.headers.origin!==origin)){send(403,{error:'origin_rejected'});return true;}
   if(!['/api/device/pair/begin','/api/device/pair/poll'].includes(url.pathname)||url.search){send(404,{error:'not_found'});return true;}
   if(req.method!=='POST'){send(405,{error:'post_required'});return true;}
   if(!/^application\/json(?:;|$)/i.test(req.headers['content-type']||'')){send(415,{error:'json_required'});return true;}
   let bytes=Buffer.alloc(0);
   for await(const part of req.iterator({destroyOnReturn:false})){if(bytes.length+part.length>1024){res.setHeader('Connection','close');send(413,{error:'request_too_large'});return true;}bytes=Buffer.concat([bytes,part]);}
   let body;try{body=JSON.parse(bytes.toString('utf8'));}catch{send(400,{error:'invalid_request'});return true;}
   if(!body||typeof body!=='object'||Array.isArray(body)){send(400,{error:'invalid_request'});return true;}
   // Direct peer only: never use a caller-supplied IP or limiter key. Behind a
   // tunnel these limits deliberately apply to the shared proxy until configured.
   const subject=req.socket.remoteAddress||'unknown';
   if(url.pathname.endsWith('/begin')){
    if(Object.keys(body).some(k=>!['label','requestedScopes'].includes(k))){send(400,{error:'invalid_request'});return true;}
    send(200,{...pairing.begin({...body,subject}),verificationUri:origin+'/#account'});
   }else{
    if(Object.keys(body).some(k=>k!=='deviceCode')){send(400,{error:'invalid_request'});return true;}
    send(200,pairing.poll(body.deviceCode,subject));
   }
  }catch(e){const known=['invalid_request','invalid_device_code','rate_limited','temporarily_unavailable'];send(e.code==='rate_limited'?429:e.code==='temporarily_unavailable'?503:known.includes(e.code)?400:500,{error:known.includes(e.code)?e.code:'pairing_failed'});}
  return true;
 };
}
