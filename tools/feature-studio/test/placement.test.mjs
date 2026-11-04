import {test} from 'node:test';
import assert from 'node:assert/strict';
import {visibleBlockSpot,canvasFrame} from '../public/graph-layout.js';

const intersects=(a,b)=>a.x<b.x+b.w&&a.x+a.w>b.x&&a.y<b.y+b.h&&a.y+a.h>b.y;
test('new blocks are revealed outside the tray, brief, and zoom controls across zoom levels',()=>{
  const viewport={x:20,y:180,w:850,h:550};
  const obstacles=[{x:36,y:360,w:340,h:320},{x:36,y:190,w:170,h:50},{x:720,y:680,w:130,h:38}];
  for(const zoom of [.4,.75,1,1.5]) {
    const size={w:184*zoom+16,h:118*zoom+22};
    const result=visibleBlockSpot(viewport,obstacles,size,{x:100,y:380});
    assert.ok(result);
    assert.ok(obstacles.every(o=>!intersects(result,o)));
    assert.ok(result.x>=viewport.x&&result.y>=viewport.y);
    assert.ok(result.x+result.w<=viewport.x+viewport.w&&result.y+result.h<=viewport.y+viewport.h);
  }
});
test('a full-height tray leaves room beside it, and an obstructed small viewport requests a collapsed tray',()=>{
  const tray={x:10,y:10,w:340,h:490};
  assert.ok(visibleBlockSpot({x:0,y:0,w:720,h:520},[tray],{w:200,h:140},{x:50,y:80}).x>=364);
  assert.equal(visibleBlockSpot({x:0,y:0,w:380,h:500},[tray],{w:200,h:140}),null);
});
test('extra pan slack can place even the first world block on the unobscured right side',()=>{
  const bounds={x:0,y:0,w:300,h:220},frame=canvasFrame(bounds,1,720,520);
  const target=visibleBlockSpot({x:0,y:0,w:720,h:520},[{x:10,y:10,w:340,h:490}],{w:200,h:140},{x:20,y:30});
  const scrollLeft=frame.x+35-(target.x+8);
  assert.ok(scrollLeft>=0);
  assert.ok(scrollLeft<=frame.width-720);
});
