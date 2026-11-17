import test from 'node:test';
import assert from 'node:assert/strict';
import {readRoute,routeHref,tabsFor} from '../src/navigation.mjs';

test('navigation links restore pages and tabs from the URL',()=>{
  assert.deepEqual(readRoute(routeHref('plugins','updates')),{page:'plugins',tab:'updates'});
  assert.deepEqual(readRoute('#settings'),{page:'settings',tab:'behavior'});
  assert.deepEqual(readRoute('#settings/invalid'),{page:'settings',tab:'behavior'});
  assert.deepEqual(readRoute('#logs'),{page:'maintenance',tab:'logs'});
  assert.deepEqual(readRoute('#updates'),{page:'maintenance',tab:'updates'});
  assert.deepEqual(readRoute('#tasks'),{page:'maintenance',tab:'tasks'});
  assert.equal(readRoute('#unknown').page,'overview');
  assert.deepEqual(readRoute(''),{page:'overview'});
  assert.deepEqual(readRoute('#overview/diagnostics'),{page:'maintenance',tab:'diagnostics'});
  assert.deepEqual(readRoute('#overview/stability'),{page:'maintenance',tab:'stability'});
  assert.deepEqual(readRoute(routeHref('maintenance','logs')),{page:'maintenance',tab:'logs'});
  assert.equal(readRoute('#plugins/updates/more').page,'overview');
  assert.deepEqual(tabsFor('constructor'),[]);
});
