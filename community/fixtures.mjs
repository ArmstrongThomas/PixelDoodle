import { deflateSync } from 'node:zlib';
const palette = ['#162e3c','#244c58','#347c77','#65b99e','#c8dfaa','#f4dfac','#eaa45d','#bd5c56','#735775','#b5a6b7'];
const rgba = hex => [...hex.slice(1).match(/../g).map(n=>parseInt(n,16)),255];
function crc(b){let c=0xffffffff;for(const v of b){c^=v;for(let k=0;k<8;k++)c=(c>>>1)^((c&1)?0xedb88320:0);}return (c^0xffffffff)>>>0;}
function chunk(type,data){const b=Buffer.alloc(data.length+12);b.writeUInt32BE(data.length);b.write(type,4);data.copy(b,8);b.writeUInt32BE(crc(b.subarray(4,-4)),b.length-4);return b;}
export function png(scene,variant=0,width=64,height=64){
 const size=64,p=Buffer.alloc(size*size*4);const colors=palette.map(rgba);
 const pixel=(x,y,c)=>{if(x>=0&&x<size&&y>=0&&y<size)Buffer.from(colors[c]).copy(p,(y*size+x)*4);};
 const rect=(x,y,w,h,c)=>{for(let yy=y;yy<y+h;yy++)for(let xx=x;xx<x+w;xx++)pixel(xx,yy,c);};
 const disk=(x,y,r,c)=>{for(let yy=-r;yy<=r;yy++)for(let xx=-r;xx<=r;xx++)if(xx*xx+yy*yy<=r*r)pixel(x+xx,y+yy,c);};
 rect(0,0,64,64,scene===3?4:0);
 if(scene===0){ // Tiny mountain lookout
  disk(48,13,7,5);for(let x=0;x<64;x++){let h=Math.floor(15+15*Math.abs(Math.sin(x/19)));rect(x,64-h,1,h,1);let h2=Math.floor(8+12*Math.abs(Math.sin((x+12)/14)));rect(x,64-h2,1,h2,2);}
  rect(16,30,24,21,6);rect(19,34,18,17,5);for(let y=0;y<12;y++)rect(12+y,30-y,32-y*2,1,7);rect(25,39,7,12,1);rect(34,35,5,6,0);rect(10,51,41,3,3);rect(43,24,3,27,1);for(let y=0;y<15;y++)rect(36+Math.floor(y/3),28+y,18-Math.floor(y/3)*2,1,3);
 }else if(scene===1){ // Mushroom grove
  rect(0,0,64,64,1);rect(0,49,64,15,2);rect(12,47,40,3,3);rect(26,30,11,20,5);rect(29,30,4,18,4);disk(31,28,18,7);rect(10,29,43,7,7);rect(10,35,43,3,6);rect(21,17,6,5,5);rect(34,13,5,5,5);rect(40,25,6,5,5);rect(18,28,4,4,5);rect(46,45,4,8,5);disk(48,43,7,6);rect(41,43,15,3,6);
 }else if(scene===2){ // Friendly robot
  rect(0,0,64,64,8);rect(11,52,42,4,0);rect(21,18,24,21,3);rect(18,21,30,14,3);rect(23,22,20,11,0);rect(26,25,4,4,5);rect(36,25,4,4,5);rect(30,31,7,2,4);rect(31,12,3,6,5);disk(32,10,3,6);rect(23,40,20,10,2);rect(26,50,5,6,4);rect(36,50,5,6,4);rect(17,40,4,9,4);rect(45,40,4,9,4);rect(28,42,9,4,6);
 }else if(scene===3){ // Potted fern
  rect(0,52,64,12,5);rect(23,39,20,4,6);rect(26,43,14,12,7);rect(29,44,4,10,6);rect(31,15,3,24,1);for(let y=15;y<38;y+=7){rect(20,y,12,4,2);rect(18,y-3,8,4,3);rect(34,y+2,12,4,2);rect(40,y-1,8,4,3);}rect(29,10,7,8,3);
 }else if(scene===4){ // Floating island
  disk(48,12,6,5);for(let y=0;y<18;y++)rect(12+y,37+y,40-y*2,1,y<4?3:1);rect(11,32,42,6,3);rect(15,29,34,4,4);rect(28,17,13,13,6);for(let y=0;y<8;y++)rect(24+y,17-y,22-y*2,1,7);rect(32,22,5,8,1);rect(18,22,3,8,1);disk(19,18,7,2);rect(41,36,5,28,2);rect(43,36,2,28,3);
 }else { // Sea creature
  rect(0,0,64,64,1);for(let y=8;y<64;y+=12)rect(0,y,64,2,2);disk(32,33,16,3);rect(16,25,28,15,3);rect(43,28,9,6,3);for(let y=0;y<12;y++)rect(11,27+y,8-Math.abs(y-6),1,4);rect(36,27,4,4,0);rect(37,27,1,1,5);rect(24,31,2,9,2);disk(48,15,3,4);disk(53,7,2,4);rect(0,56,64,8,5);
 }
 // Small, deterministic star accents; each variant has its own arrangement.
 if(scene!==3)for(let i=0;i<8;i++){let x=(i*19+variant*11+7)%64,y=(i*7+variant*3+3)%23;if(p[(y*64+x)*4]===colors[0][0])pixel(x,y,4);}
 // Center the scene in a real rectangular canvas; never stretch its pixels.
 const raw=Buffer.alloc(height*(1+width*4));
 const scale=Math.min(1,width/64,height/64),sw=Math.round(64*scale),sh=sw;
 const ox=Math.floor((width-sw)/2),oy=Math.floor((height-sh)/2);
 const background=colors[scene===2?8:scene===3?4:scene===1||scene===5?1:0];
 for(let y=0;y<height;y++)for(let x=0;x<width;x++){
  const at=y*(1+width*4)+1+x*4;
  if(x>=ox&&x<ox+sw&&y>=oy&&y<oy+sh){const src=(Math.floor((y-oy)/scale)*64+Math.floor((x-ox)/scale))*4;p.copy(raw,at,src,src+4);}
  else Buffer.from(background).copy(raw,at);
 }
 const hdr=Buffer.alloc(13);hdr.writeUInt32BE(width);hdr.writeUInt32BE(height,4);hdr[8]=8;hdr[9]=6;
 return Buffer.concat([Buffer.from([137,80,78,71,13,10,26,10]),chunk('IHDR',hdr),chunk('IDAT',deflateSync(raw)),chunk('IEND',Buffer.alloc(0))]);
}
const titles=['Last light at the lookout','Under the mushroom canopy','A friend from orbit','A quiet windowsill','Somewhere above the clouds','Going with the current','The long way home','Forest floor discoveries','Ready for a little adventure','Room to grow','An island of your own','Blue hour drifting'];
const tags=[['landscape','cozy'],['nature','cozy'],['characters','sci-fi'],['nature','objects'],['landscape','fantasy'],['characters','nature']];
export const artists=[{id:'demo-studio',name:'Demo Studio',bio:'Synthetic pixel studies made for this local prototype. These are not community submissions.'},{id:'demo-workshop',name:'Demo Workshop',bio:'Procedural sample artwork for testing gallery browsing and handheld PNG downloads.'}];
export const artworks=titles.map((title,i)=>({id:`study-${i+1}`,title,artist:artists[i%2].id,tags:tags[i%6],width:[64,64,64,48,96,96][i%6],height:[64,96,64,80,64,48][i%6],status:'published',visibility:'public',publishedAt:new Date(Date.UTC(2026,9,15,12-i)).toISOString(),scene:i%6,variant:i}));
// Deliberate non-public fixtures exercise every public read boundary.
for(const status of ['pending','denied','held'])artworks.push({...artworks[0],id:`hidden-${status}`,title:`Private ${status}`,status});
artworks.push({...artworks[0],id:'hidden-private',visibility:'private'});
