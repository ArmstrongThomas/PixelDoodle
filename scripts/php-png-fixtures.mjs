// Migration oracle fixtures only; production PHP does not need Node.
import {writeFileSync,mkdirSync} from 'node:fs';import {deflateSync} from 'node:zlib';import {chunk,decodePNG} from '../community/png.mjs';
const sig=Buffer.from([137,80,78,71,13,10,26,10]);const cases=[];
for(const type of [0,2,3,4,6])for(let filter=0;filter<5;filter++){
 const bpp={0:1,2:3,3:1,4:2,6:4}[type],w=3,h=2,stride=w*bpp;
 const rows=Buffer.from(Array.from({length:stride*h},(_,i)=>type===3?i%3:(i*43+19)&255));const raw=Buffer.alloc((stride+1)*h);
 for(let y=0;y<h;y++){raw[y*(stride+1)]=filter;for(let x=0;x<stride;x++){const a=x>=bpp?rows[y*stride+x-bpp]:0,u=y?rows[(y-1)*stride+x]:0,c=y&&x>=bpp?rows[(y-1)*stride+x-bpp]:0,p=a+u-c,pa=Math.abs(p-a),pb=Math.abs(p-u),pc=Math.abs(p-c),pred=[0,a,u,Math.floor((a+u)/2),pa<=pb&&pa<=pc?a:pb<=pc?u:c][filter];raw[y*(stride+1)+x+1]=(rows[y*stride+x]-pred)&255;}}
 const hdr=Buffer.alloc(13);hdr.writeUInt32BE(w);hdr.writeUInt32BE(h,4);hdr[8]=8;hdr[9]=type;
 const extras=type===3?[chunk('PLTE',Buffer.from([1,2,3,4,5,6,7,8,9])),chunk('tRNS',Buffer.from([0,127,255]))]:[];
 const png=Buffer.concat([sig,chunk('IHDR',hdr),...extras,chunk('IDAT',deflateSync(raw)),chunk('IEND',Buffer.alloc(0))]);
 cases.push({name:`type-${type}-filter-${filter}`,png:png.toString('base64'),rgba:decodePNG(png).rgba.toString('base64'),width:w,height:h});
}
mkdirSync(new URL('../build/',import.meta.url),{recursive:true});writeFileSync(new URL('../build/php-png-fixtures.json',import.meta.url),JSON.stringify(cases));
