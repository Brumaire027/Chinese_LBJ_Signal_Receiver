#include "reception_debug.hpp"
#include "networks.hpp"
#include "status_display.hpp"
#include "task_state.hpp"
#include "receiver_control.hpp"
#include <atomic>
#include <cstdio>

extern SX1276 radio;
namespace {
std::atomic<bool> enabled{false};
std::atomic<RxDebugMode> mode{RxDebugMode::Normal};
std::atomic<uint32_t> maxima[unsigned(RxDebugStage::Count)]{};
std::atomic<uint32_t> unknown{0};
uint32_t batches=0, errors=0, damaged=0, peak=0, busyPeak=0, growth=0;
uint32_t syncBase=0, dropBase=0, resetBase=0, discardBase=0, began=0;
uint32_t syncs=0, fullDrops=0, resets=0, discarded=0;
uint32_t stoppedElapsed=0;
bool closePending=false;
int16_t lastError=0;
int previousBytes=0;
bool wasBusy=false;
uint8_t selected=0, page=0;
bool viewing=false;
const char *notice=nullptr;
void maximum(std::atomic<uint32_t> &out,uint32_t value) {
    uint32_t old=out.load();
    while(old<value && !out.compare_exchange_weak(old,value)) {}
}
void clearStats() {
    for(auto &v:maxima) v.store(0);
    unknown=0; batches=errors=damaged=peak=busyPeak=growth=0;
    syncs=fullDrops=resets=discarded=0; lastError=0; wasBusy=false; previousBytes=0;
    auto &d=radio.directDiagnostics;
    syncBase=d.syncs.load(); dropBase=d.fullDrops.load(); resetBase=d.resets.load(); discardBase=d.discardedBytes.load();
    d.peak.exchange(0); began=millis(); stoppedElapsed=0;
}
const char *modeText() {
    return mode.load()==RxDebugMode::NoSd ? "暂停SD记录" : mode.load()==RxDebugMode::NoLog ? "关闭详细日志" : "正常记录";
}
void timing(char *line,size_t size,const char *name,RxDebugStage stage) {
    const uint32_t us=maxima[unsigned(stage)].load();
    snprintf(line,size,"%s:%lu.%lu毫秒",name,(unsigned long)(us/1000),(unsigned long)(us%1000/100));
}
}
bool rxDebugEnabled() { return enabled.load(); }
bool rxDebugAllowLog() { return rxDebugLogPolicy(enabled.load(),mode.load()); }
bool rxDebugAllowCsv() { return rxDebugCsvPolicy(enabled.load(),mode.load()); }
RxDebugTimer::RxDebugTimer(RxDebugStage value):stage(value),started(0),active(rxDebugEnabled()) {
    if(active) started=micros();
}
RxDebugTimer::~RxDebugTimer() {
    if(active) maximum(maxima[unsigned(stage)],rxDebugElapsed(micros(),started));
}
void noteRxDebugBatch(int16_t status,const data_bond &bond) {
    if(!rxDebugEnabled()) return;
    ++batches;
    if(status!=RADIOLIB_ERR_NONE) { ++errors; lastError=status; }
    for(const auto &p:bond.pocsagData) if(!p.is_empty && (p.errs_uncorrected || p.str.indexOf('X')>=0)) {
        ++damaged; break;
    }
}
void noteRxDebugDecoded(int type) {
    if(rxDebugEnabled() && type<0) unknown.fetch_add(1);
}
void updateReceptionDebug() {
    if(closePending && fd_state.load()==TASK_INIT) { closeReceptionDebug(); return; }
    if(!rxDebugEnabled()) return;
    auto &d=radio.directDiagnostics;
    syncs=d.syncs.load()-syncBase; fullDrops=d.fullDrops.load()-dropBase;
    resets=d.resets.load()-resetBase; discarded=d.discardedBytes.load()-discardBase;
    const uint32_t latest=d.peak.exchange(0);
    if(latest>peak) peak=latest;
    const int bytes=radio.available();
    const bool busy=fd_state.load()!=TASK_INIT;
    if(busy) {
        if(bytes>int(busyPeak)) busyPeak=bytes;
        if(wasBusy && bytes>previousBytes) ++growth;
    }
    previousBytes=bytes; wasBusy=busy;
}
void openReceptionDebug() { selected=page=0; viewing=false; notice=nullptr; }
void closeReceptionDebug() {
    // Finish a record under the same output policy it started with.
    if(fd_state.load()!=TASK_INIT) { closePending=true; return; }
    closePending=false;
    radio.directDiagnostics.enabled=false;
    updateReceptionDebug();
    if(rxDebugEnabled()) stoppedElapsed=rxDebugElapsed(millis(),began);
    // Runtime overrides only: persisted logging settings are never changed.
    enabled=false; radio.directDiagnostics.enabled=false; mode=RxDebugMode::Normal;
    viewing=false; notice=nullptr;
}
bool handleReceptionDebugButton(ButtonId key) {
    if(key==ButtonId::Key4) {
        if(viewing) { viewing=false; return false; }
        closeReceptionDebug(); return true;
    }
    if(viewing) {
        if(key==ButtonId::Key2) page=(page+5)%6;
        if(key==ButtonId::Key3 || key==ButtonId::Key1) page=(page+1)%6;
        return false;
    }
    notice=nullptr;
    if(key==ButtonId::Key2) selected=(selected+3)%4;
    if(key==ButtonId::Key3) selected=(selected+1)%4;
    if(key!=ButtonId::Key1) return false;
    if(selected==1) { viewing=true; page=0; return false; }
    // No switching/clearing midway through a formatted record or a scoped timer.
    if(fd_state.load()!=TASK_INIT) { notice="接收处理中，请稍后"; return false; }
    if(selected==0) {
        if(rxDebugEnabled()) closeReceptionDebug();
        else { clearStats(); mode=RxDebugMode::Normal; enabled=true; radio.directDiagnostics.enabled=true; }
    } else if(selected==2) { clearStats(); notice="统计已清零"; }
    else if(selected==3) {
        if(!rxDebugEnabled()) notice="请先开启诊断";
        else {
            mode=RxDebugMode((unsigned(mode.load())+1)%3);
            clearStats(); // Do not mix measurements from different test conditions.
        }
    }
    return false;
}
void renderReceptionDebug() {
    char title[64], a[64], b[64], c[64], d[64];
    if(!viewing) {
        snprintf(a,sizeof(a),"诊断:%s",rxDebugEnabled()?"开":"关");
        snprintf(d,sizeof(d),"方式:%s",modeText());
        const char *lines[]={a,"查看统计",notice?notice:"清零统计",d};
        showMenuScreen("接收诊断",lines,4,selected,true); return;
    }
    snprintf(title,sizeof(title),"诊断 %u/6 %s",unsigned(page+1),
        !rxDebugEnabled()?"关":mode.load()==RxDebugMode::NoSd?"停写":mode.load()==RxDebugMode::NoLog?"无日志":"正常");
    switch(page) {
    case 0:
        snprintf(a,sizeof(a),"同步次数:%lu",(unsigned long)syncs);
        snprintf(b,sizeof(b),"读取批次:%lu",(unsigned long)batches);
        snprintf(c,sizeof(c),"读取失败:%lu",(unsigned long)errors);
        snprintf(d,sizeof(d),"最近错误码:%d",lastError); break;
    case 1:
        snprintf(a,sizeof(a),"缓存峰值:%lu/256",(unsigned long)peak);
        snprintf(b,sizeof(b),"满缓冲丢弃:%lu",(unsigned long)fullDrops);
        snprintf(c,sizeof(c),"同步清除:%lu次",(unsigned long)resets);
        snprintf(d,sizeof(d),"清除字节:%lu",(unsigned long)discarded); break;
    case 2:
        timing(a,sizeof(a),"解码最大",RxDebugStage::Raw);
        timing(b,sizeof(b),"解析最大",RxDebugStage::Parse);
        timing(c,sizeof(c),"日志最大",RxDebugStage::Log);
        timing(d,sizeof(d),"CSV最大",RxDebugStage::Csv); break;
    case 3:
        timing(a,sizeof(a),"随车最大",RxDebugStage::Ride);
        timing(b,sizeof(b),"输出最大",RxDebugStage::Batch);
        snprintf(c,sizeof(c),"忙时缓存:%lu字节",(unsigned long)busyPeak);
        snprintf(d,sizeof(d),"缓存增长:%lu次",(unsigned long)growth); break;
    case 4:
        snprintf(a,sizeof(a),"含坏码批次:%lu",(unsigned long)damaged);
        snprintf(b,sizeof(b),"未知报文:%lu",(unsigned long)unknown.load());
        snprintf(c,sizeof(c),"统计时长:%lu秒",(unsigned long)((rxDebugEnabled()?rxDebugElapsed(millis(),began):stoppedElapsed)/1000));
        snprintf(d,sizeof(d),"上下翻页 返回退出"); break;
    default: {
        sampleReceiverDiagnostics();
        const auto &r=receiverDiagnostics();
        snprintf(a,sizeof(a),"频率:%.4fMHz",actual_frequency);
        if(r.haveRssi && millis64()-r.sampledMs<2000)
            snprintf(b,sizeof(b),"信号:%.0fdBm",r.rssi);
        else snprintf(b,sizeof(b),"信号:等待采样");
        snprintf(c,sizeof(c),"接收:%s",fd_state.load()==TASK_INIT?"空闲":"处理中");
        snprintf(d,sizeof(d),"%s",modeText()); break;
    }
    }
    const char *lines[]={a,b,c,d}; showMenuScreen(title,lines,4,0,false);
}
