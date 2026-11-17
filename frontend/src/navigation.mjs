export const pages = [
  ['overview','概览'], ['settings','菜单设置'], ['plugins','插件管理'], ['maintenance','维护'],
];

const tabs = {
  settings:[['behavior','常用'], ['keys','快捷键'], ['advanced','高级']],
  plugins:[['installed','插件与版本'], ['updates','离线安装']],
  maintenance:[['updates','应用更新'], ['diagnostics','诊断'], ['stability','稳定性'], ['tasks','最近任务'], ['logs','运行日志']],
};

export const tabsFor = page => Object.hasOwn(tabs,page) ? tabs[page] : [];

export function readRoute(hash) {
  const [page,tab,extra] = hash.replace(/^#/,'').split('/');
  if(extra!==undefined)return {page:'overview'};
  if(['updates','tasks','logs'].includes(page) && tab===undefined)return {page:'maintenance',tab:page};
  if(page==='overview' && ['diagnostics','stability'].includes(tab))return {page:'maintenance',tab};
  if(!pages.some(([name])=>name===page))return {page:'overview'};
  const choices=tabsFor(page);
  return {page,tab:choices.find(([name])=>name===tab)?.[0] ?? choices[0]?.[0]};
}

export function routeHref(page,tab) {
  return tab ? `#${page}/${tab}` : `#${page}`;
}
