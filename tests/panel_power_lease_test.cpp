#include "panel_power_lease.hpp"
#include <iostream>
#include <stdexcept>
using wordcard::PanelPowerLease;
int main(){try{
 unsigned checks=0;auto check=[&](bool b){++checks;if(!b)throw std::runtime_error("panel power lease");};
 PanelPowerLease p;check(!p.held());check(!p.expired(9000000));
 // A failed scan or a USB/shutdown screen must not retain a lease, including
 // failures following a successful warm burst.
 for(bool success:{false,true})for(bool eligible:{false,true}){
  p.completed(1000,true,true);p.completed(2000,success,eligible);
  check(p.held()==(success&&eligible));
 }
 p.completed(1000,true,true);check(!p.expired(4000999));check(p.expired(4001000));
 // The deadline is renewed by a completed frame, not by a touch/paint request.
 for(uint64_t i=0;i<20;i++){
  const auto now=10000000+i*900000;p.completed(now,true,true);
  check(p.held());check(!p.expired(now+3999999));check(p.expired(now+4000000));
 }
 p.release();check(!p.held());check(!p.expired(UINT64_C(100000000)));
 p.completed(UINT64_C(1)<<40,true,true);check(!p.expired((UINT64_C(1)<<40)+3999999));check(p.expired((UINT64_C(1)<<40)+4000000));
 p.completed(UINT64_C(1)<<40,false,true);check(!p.held());
 std::cout<<"PASS: "<<checks<<" panel lease lifetime / failure / boundary checks\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
