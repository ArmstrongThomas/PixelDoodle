import {accountRoutes} from './account-routes.mjs';
import {deviceRoutes} from './device-routes.mjs';
import http from 'node:http';
import { readFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import { fixtureRepository, openRepository } from './repository.mjs';
const publicRoot=new URL('./public/',import.meta.url);
const visible=a=>a.status==='published'&&a.visibility==='public';
const summary=(a,artists)=>({id:a.id,title:a.title,artist:artists.find(p=>p.id===a.artist),tags:a.tags,width:a.width,height:a.height,publishedAt:a.publishedAt,image:`/media/${a.id}.png`,thumbnail:`/thumbs/${a.id}.png`,handheld:`/thumbs/${a.id}.png?download=1`,download:`/media/${a.id}.png?download=1`,synthetic:a.synthetic!==false});
const headers={'X-Content-Type-Options':'nosniff','Referrer-Policy':'no-referrer','Cache-Control':'no-store','Content-Security-Policy':"default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self'; connect-src 'self'; object-src 'none'; base-uri 'none'; frame-ancestors 'none'"};
export function createServer(repository=fixtureRepository(),accountConfig){const accounts=accountRoutes(accountConfig),devices=deviceRoutes(accountConfig);return http.createServer(async(req,res)=>{
 const send=(status,body,type='application/json; charset=utf-8',extra={})=>{res.writeHead(status,{...headers,'Content-Type':type,...extra});res.end(req.method==='HEAD'?undefined:(type.startsWith('application/json')?JSON.stringify(body):body));};
 try{
  if(await devices(req,res)||await accounts(req,res))return;
  if(!['GET','HEAD'].includes(req.method))return send(405,{error:'Read-only gallery prototype' },undefined,{'Allow':'GET, HEAD'});
  const url=new URL(req.url,'http://localhost');const path=url.pathname;
  const {artworks,artists}=repository.snapshot();
  if(path==='/api/gallery'){
   const q=(url.searchParams.get('q')||'').trim().toLowerCase(),tag=url.searchParams.get('tag')||'',artist=url.searchParams.get('artist')||'';
   const limit=Number(url.searchParams.get('limit')||12),offset=Number(url.searchParams.get('offset')||0);
   if(q.length>100||tag.length>32||artist.length>64||!Number.isInteger(limit)||limit<1||limit>24||!Number.isSafeInteger(offset)||offset<0||offset>10000)return send(400,{error:'Invalid gallery query'});
   const all=artworks.filter(visible);const found=all.filter(a=>(!q||`${a.title} ${a.tags.join(' ')} ${artists.find(p=>p.id===a.artist).name}`.toLowerCase().includes(q))&&(!tag||a.tags.includes(tag))&&(!artist||a.artist===artist)).sort((a,b)=>b.publishedAt.localeCompare(a.publishedAt)||a.id.localeCompare(b.id));
   return send(200,{version:1,mode:'synthetic-local-preview',items:found.slice(offset,offset+limit).map(a=>summary(a,artists)),total:found.length,nextOffset:offset+limit<found.length?offset+limit:null,tags:[...new Set(all.flatMap(a=>a.tags))].sort()});
  }
  const art=path.match(/^\/api\/artworks\/([a-z0-9-]+)$/),media=path.match(/^\/(?:media|thumbs)\/([a-z0-9-]+)\.png$/);
  if(art||media){const item=artworks.find(a=>a.id===(art||media)[1]&&visible(a));if(!item)return send(404,{error:'Artwork not found'});
   if(art)return send(200,{...summary(item,artists),description:'A synthetic pixel study for the Pixel Doodle community prototype. Download the PNG to try it in the handheld gallery cache.'});
   const image=repository.image(item.id,path.startsWith('/thumbs/')?'thumbnail':'image');if(!image)return send(404,{error:'Artwork not found'});return send(200,image,'image/png',url.searchParams.has('download')?{'Content-Disposition':`attachment; filename="${item.id}.png"`}:{});
  }
  const person=path.match(/^\/api\/artists\/([a-z0-9-]+)$/);
  if(person){const artist=artists.find(a=>a.id===person[1]);if(!artist||!artworks.some(a=>a.artist===artist.id&&visible(a)))return send(404,{error:'Artist not found'});return send(200,{...artist,synthetic:true,count:artworks.filter(a=>a.artist===artist.id&&visible(a)).length});}
  const files={'/studio.js':['studio.js','text/javascript; charset=utf-8'],'/account.js':['account.js','text/javascript; charset=utf-8'],'/':['index.html','text/html; charset=utf-8'],'/app.js':['app.js','text/javascript; charset=utf-8'],'/style.css':['style.css','text/css; charset=utf-8']};
  if(files[path]){const [name,type]=files[path];return send(200,await readFile(new URL(name,publicRoot)),type);}
  return send(404,{error:'Not found'});
 }catch {if(!res.headersSent)send(500,{error:'Unable to load gallery'});else res.end();}
});}
if(process.argv[1]===fileURLToPath(import.meta.url)){
 const port=Number(process.env.PORT||4178);if(!Number.isInteger(port)||port<0||port>65535)throw Error('Invalid PORT');
 const host=process.env.PREVIEW_HOST||'127.0.0.1';
 const repository=process.env.GALLERY_DATABASE?openRepository(process.env.GALLERY_DATABASE):fixtureRepository();
 const server=createServer(repository);server.on('close',()=>repository.close());server.listen(port,host,()=>console.log(`Preview: http://${host}:${server.address().port}`));
 server.on('error',e=>{console.error(e.code==='EADDRINUSE'?'Port occupied; choose another PORT.':e.message);process.exitCode=1;});
}
