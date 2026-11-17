import test from 'node:test';
import assert from 'node:assert/strict';
import {readRoute,routeHref,tabsFor} from '../src/navigation.mjs';

test('navigation links restore pages and tabs from the URL',()=>{
  assert.deepEqual(readRoute(routeHref('plugins','updates')),{page:'plugins',tab:'updates'});
  assert.deepEqual(readRoute('#settings'),{page:'settings',tab:'behavior'});
  assert.deepEqual(readRoute('#settings/invalid'),{page:'settings',tab:'behavior'});
  assert.equal(readRoute('#logs').page,'logs');
  assert.equal(readRoute('#unknown').page,'overview');
  assert.equal(readRoute('#plugins/updates/more').page,'overview');
  assert.deepEqual(tabsFor('constructor'),[]);
});
