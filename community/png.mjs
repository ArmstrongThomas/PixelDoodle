/** Bounded PNG subset for Pixel Doodle exports. See W3C PNG 3, sections 5/9/11. */
import {inflateSync,deflateSync} from 'node:zlib';
export const MAX_PNG_BYTES=2*1024*1024;
const signature=Buffer.from([137,80,78,71,13,10,26,10]);
export function crc32(bytes){let c=0xffffffff;for(const b of bytes){c^=b;for(let i=0;i<8;i++)c=(c>>>1)^((c&1)?0xedb88320:0);}return (c^0xffffffff)>>>0;}
export function chunk(type,data){const b=Buffer.alloc(data.length+12);b.writeUInt32BE(data.length);b.write(type,4);data.copy(b,8);b.writeUInt32BE(crc32(b.subarray(4,-4)),b.length-4);return b;}
const fail=message=>{throw Error(message);};
export function decodePNG(b){
 if(!Buffer.isBuffer(b)||b.length>MAX_PNG_BYTES||b.length<45||!b.subarray(0,8).equals(signature))fail('Expected a PNG no larger than 2 MiB');
 let w,h,type,bpp,palette,alpha,ended=false,dataStarted=false,dataEnded=false;const parts=[];let count=0;
 for(let at=8;at<b.length;){
  if(++count>1024||at+12>b.length)fail('Invalid PNG chunk bounds');const n=b.readUInt32BE(at),t=b.toString('latin1',at+4,at+8);
  if(n>b.length-at-12||! /^[A-Za-z]{2}[A-Z][A-Za-z]$/.test(t))fail('Invalid PNG chunk');
  const d=b.subarray(at+8,at+8+n);if(crc32(b.subarray(at+4,at+8+n))!==b.readUInt32BE(at+8+n))fail('PNG checksum mismatch');at+=n+12;
  if(!w&&t!=='IHDR')fail('PNG header must be first');
  if(dataStarted&&t!=='IDAT')dataEnded=true;
  if(t==='IHDR'){
   if(w||n!==13)fail('Invalid PNG header');w=d.readUInt32BE(0);h=d.readUInt32BE(4);type=d[9];bpp=({0:1,2:3,3:1,4:2,6:4})[type];
   if(w<1||h<1||w>512||h>512)fail('Canvas must be 1–512 pixels per side');
   if(d[8]!==8||!bpp||d[10]||d[11]||d[12])fail('Use a non-interlaced 8-bit PNG (RGB, RGBA, grayscale or indexed)');
  }else if(t==='PLTE'){
   if(palette||dataStarted||alpha||type===0||type===4||!n||n%3||n>768)fail('Invalid PNG palette');palette=d;
  }else if(t==='tRNS'){
   if(alpha||dataStarted||![0,2,3].includes(type)||(type===3&&(!palette||!n||n>palette.length/3))||(type===0&&(n!==2||d.readUInt16BE(0)>255))||(type===2&&(n!==6||[0,2,4].some(i=>d.readUInt16BE(i)>255))))fail('Invalid PNG transparency');alpha=d;
  }else if(t==='IDAT'){
   if(dataEnded||(type===3&&!palette))fail('Invalid PNG image ordering');dataStarted=true;parts.push(d);
  }else if(t==='IEND'){
   if(n||!dataStarted||at!==b.length)fail('Invalid PNG end');ended=true;break;
  }else if(['acTL','fcTL','fdAT','iCCP','cICP','mDCV','cLLI'].includes(t))fail('Animated/HDR/profile PNGs are not supported; export a static standard RGB PNG');
  else if(t[0]===t[0].toUpperCase())fail('Unsupported critical PNG chunk');
  // Other ancillary metadata is bounded, CRC checked, never interpreted, and stripped.
 }
 if(!ended)fail('Incomplete PNG');
 const stride=w*bpp,expected=(stride+1)*h,compressed=Buffer.concat(parts);let decoded;
 try{decoded=inflateSync(compressed,{maxOutputLength:expected,info:true});}catch{fail('Invalid or oversized PNG compressed data');}
 if(decoded.buffer.length!==expected||decoded.engine.bytesWritten!==compressed.length)fail('PNG data length mismatch');
 const raw=decoded.buffer,rows=Buffer.alloc(stride*h),rgba=Buffer.alloc(w*h*4);
 for(let y=0;y<h;y++){
  const filter=raw[y*(stride+1)];if(filter>4)fail('Invalid PNG row filter');
  for(let x=0;x<stride;x++){
   const a=x>=bpp?rows[y*stride+x-bpp]:0,c=y&&x>=bpp?rows[(y-1)*stride+x-bpp]:0,up=y?rows[(y-1)*stride+x]:0;
   const p=a+up-c,pa=Math.abs(p-a),pb=Math.abs(p-up),pc=Math.abs(p-c);
   const pred=filter===0?0:filter===1?a:filter===2?up:filter===3?Math.floor((a+up)/2):pa<=pb&&pa<=pc?a:pb<=pc?up:c;
   rows[y*stride+x]=(raw[y*(stride+1)+1+x]+pred)&255;
  }
 }
 for(let i=0;i<w*h;i++){
  const s=i*bpp,o=i*4;let r,g,bl,a=255;
  if(type===6){[r,g,bl,a]=rows.subarray(s,s+4);}else if(type===2){[r,g,bl]=rows.subarray(s,s+3);if(alpha&&r===alpha.readUInt16BE(0)&&g===alpha.readUInt16BE(2)&&bl===alpha.readUInt16BE(4))a=0;}
  else if(type===3){const idx=rows[s];if(idx>=palette.length/3)fail('PNG palette index out of range');[r,g,bl]=palette.subarray(idx*3,idx*3+3);a=alpha&&idx<alpha.length?alpha[idx]:255;}
  else {r=g=bl=rows[s];if(type===4)a=rows[s+1];else if(alpha&&r===alpha.readUInt16BE(0))a=0;}
  rgba[o]=r;rgba[o+1]=g;rgba[o+2]=bl;rgba[o+3]=a;
 }
 return {width:w,height:h,rgba};
}
export function encodePNG({width,height,rgba}){
 if(!Number.isInteger(width)||!Number.isInteger(height)||width<1||height<1||width>512||height>512||rgba.length!==width*height*4)fail('Invalid RGBA canvas');
 const hdr=Buffer.alloc(13);hdr.writeUInt32BE(width);hdr.writeUInt32BE(height,4);hdr[8]=8;hdr[9]=6;
 const raw=Buffer.alloc(height*(width*4+1));for(let y=0;y<height;y++)rgba.copy(raw,y*(width*4+1)+1,y*width*4,(y+1)*width*4);
 return Buffer.concat([signature,chunk('IHDR',hdr),chunk('IDAT',deflateSync(raw)),chunk('IEND',Buffer.alloc(0))]);
}
export function thumbnail(image,maxSide=128){
 if(!Number.isInteger(maxSide)||maxSide<1||maxSide>512)fail('Invalid thumbnail size');
 const scale=Math.min(1,maxSide/Math.max(image.width,image.height));const width=Math.max(1,Math.round(image.width*scale)),height=Math.max(1,Math.round(image.height*scale)),rgba=Buffer.alloc(width*height*4);
 for(let y=0;y<height;y++)for(let x=0;x<width;x++){const source=(Math.min(image.height-1,Math.floor((y+.5)*image.height/height))*image.width+Math.min(image.width-1,Math.floor((x+.5)*image.width/width)))*4;image.rgba.copy(rgba,(y*width+x)*4,source,source+4);}
 return {width,height,rgba};
}
export function prepareArtwork(bytes){const image=decodePNG(bytes),small=thumbnail(image);return {width:image.width,height:image.height,image:encodePNG(image),thumbnail:encodePNG(small),thumbnailWidth:small.width,thumbnailHeight:small.height};}
