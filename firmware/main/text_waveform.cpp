#include "text_waveform_policy.hpp"
#include "text_waveform.hpp"
#include "epdiy.h"
#include "my_waveform.h"
#include "control_waveform.h"
#include <esp_heap_caps.h>
const EpdWaveform *wordcard_text_waveform() {
    static EpdWaveformPhases phases{};
    static const EpdWaveformPhases *ranges[]={&phases};
    static const EpdWaveformMode mode{MODE_GL16,1,ranges};
    static const EpdWaveformMode *modes[]={&mode};
    static EpdWaveform wave{};
    if(phases.luts)return &wave;
    const EpdWaveformPhases *gray=nullptr;
    for(int i=0;i<MY_WAVEFORM.num_modes;++i) {
        const auto *m=MY_WAVEFORM.mode_data[i];
        if(m->type==MODE_GL16)gray=m->range_data[0];
    }
    if(!gray || gray->phase_times)return &MY_WAVEFORM;
    auto *data=static_cast<uint8_t*>(heap_caps_malloc(gray->phases*64,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
    if(!data)return &MY_WAVEFORM;
    if(!wordcard::make_text_waveform(gray->luts,gray->phases,data,gray->phases*64)) {
        heap_caps_free(data);return &MY_WAVEFORM;
    }
    phases.phases=gray->phases;phases.luts=data;phases.phase_times=nullptr;
    wave.num_modes=1;wave.num_temp_ranges=1;wave.mode_data=modes;
    wave.temp_intervals=MY_WAVEFORM.temp_intervals;
    return &wave;
}

const EpdWaveformPhases *wordcard_control_phases() {
    static EpdWaveformPhases phases{};
    if(phases.luts)return &phases;
    const EpdWaveformPhases *gl=nullptr,*gc=nullptr;
    for(int i=0;i<MY_WAVEFORM.num_modes;++i) {
        const auto *mode=MY_WAVEFORM.mode_data[i];
        if(mode->type==MODE_GL16)gl=mode->range_data[0];
        if(mode->type==MODE_GC16)gc=mode->range_data[0];
    }
    if(!gl||!gc||gl->phases!=48||gc->phases!=48||gl->phase_times||gc->phase_times)return nullptr;
    auto *data=static_cast<uint8_t*>(heap_caps_malloc(48*64,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
    if(!data)return nullptr;
    if(!wordcard_make_control_waveform(gl->luts,gl->phases,gc->luts,gc->phases,data,48*64)) {
        heap_caps_free(data);return nullptr;
    }
    phases.phases=48;phases.luts=data;phases.phase_times=nullptr;
    return &phases;
}
