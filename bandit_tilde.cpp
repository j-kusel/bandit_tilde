/// @file
///	@ingroup 	minexamples
///	@copyright	Copyright 2018 The Min-DevKit Authors. All rights reserved.
///	@license	Use of this source code is governed by the MIT License found in the License.md file.

#include "c74_min.h"

using namespace c74::min;

namespace attr {
    enum attrs { INST, START, END, TIMESIG, OFFSET };
}

// MAKE THIS A CLASS, INITIALIZE
typedef struct Measure {
    float start { 0.0 };
    float end { 0.0 };
    int timesig { 0 };
    float offset { 0.0 };
    
    //lib::circular_storage<sample>* bank { NULL };
    sample_vector bank;
    struct Measure* next { NULL };
} measure;

typedef struct Instrument {
    bool active { false };
    size_t tail {0};
    measure *m_first { NULL };
    measure *cache { NULL };
    long prev { 0 };
    bool enabled { false };
} instrument;

#define THRESHOLD 2

class bandit : public object<bandit>, public vector_operator<> {
public:

	MIN_DESCRIPTION { "Polytempo timing object for the Bandit ecosystem." };
	MIN_TAGS		{ "audio" };
	MIN_AUTHOR		{ "J. Kusel" };
	MIN_RELATED		{ "" };
    
    bandit(const atoms& args = {}) {
        short arg_num = args.size();
        // put argument checking here!
        if (arg_num) {
            m_channels = args[0];
            for (auto i=0; i<m_channels; ++i)
                m_outlets.push_back(std::make_unique<outlet<>>(this, std::string("(signal) audio output ") + std::to_string(i), "signal"));
        }
        if (arg_num == 2) {
            filepath = std::string(args[1]);
            parser.set();
        }
        info_out = std::make_unique<outlet<thread_check::scheduler, thread_action::fifo>>(this, "(list) timing information", "list");
    }
    
    //inlet<>                                                 sig             { this, "(signal)", "signal" };
	//inlet<>													input			{ this, "(open) bandit" };
    //outlet<>                                                info_out          { this, "(list) timing information", "bang" };
	//outlet<>	info_out		{ this, "(bang) input is non-zero" };
	//outlet<thread_check::scheduler, thread_action::fifo>	output_false	{ this, "(bang) input is zero" };
    
    // deferred file loading
    queue<> parser { this,
        MIN_FUNCTION {
            std::ifstream in { this->filepath };
            short line_count { -2 };
            //short inst_count { 0 };
            for (std::string line; std::getline(in, line); )
            {
                size_t pos { 0 };
                //size_t lastpos { 0 };
                string token = "";
                short col = 0;
                instrument* inst;
                measure* meas { new measure };
                line = line + std::string(","); // add one final delimiter so we process the last column
                while ((pos = line.find(",")) != std::string::npos) {
                    token = line.substr(0, pos);
                    if (line_count == -2) { // set metadata
                        if (col == 1)
                            this->PPQ = stoi(token);
                        if (col == 3)
                            this->PPQ_tempo = stoi(token);
                    } else if (line_count >= 0) {
                        switch (col) {
                            case attr::INST: {
                                inst = &this->instruments[stoi(token)];
                                if (!inst->active) // initialize new instrument if necessary
                                    inst->active = true;
                                break;
                            }
                            case attr::START: meas->start = stof(token, NULL); break;
                            case attr::END: meas->end = stof(token, NULL); break;
                            case attr::TIMESIG: meas->timesig = stoi(token); break;
                            case attr::OFFSET: meas->offset = stof(token, NULL); break;
                        }
                    }
                    col++;
                    line.erase(0, pos+1);
                }
                // finally insert measure
                if (line_count >= 0) {
                    measure* head { inst->m_first };
                    if (head == NULL) {
                        inst->m_first = meas;
                    } else if (head->offset > meas->offset) {
                        meas->next = head;
                        inst->m_first = meas;
                    } else {
                        measure* prev = head;
                        while (head != NULL) {
                            if (head->offset > meas->offset) {
                                meas->next = head;
                                prev->next = meas;
                                break;
                            }
                            prev = head;
                            head = head->next;
                        }
                        if (head == NULL) {
                            prev->next = meas;
                        }
                    }
                    this->measure_calc(meas);
                }
                ++line_count;
            }
            if (in.is_open())
                in.close();
            this->is_loaded = true;
            cout << "loaded" << endl;
            int insts = 0;
            for (int i=0; i<8; i++) {
                if (instruments[i].active) {
                    insts++;
                    cout << "Instrument " << i << endl;
                    measure* meas = instruments[i].m_first;
                    while (meas != NULL) {
                        cout << "Measure: " << meas->offset << endl;
                        meas = meas->next;
                    }
                }
            }
            
            return {};
        }
    };
        
    message<> open { this, "open", "Open a file.",
        MIN_FUNCTION {
            string arguments = to_string(args);
            if (args.size() == 1) {
                this->filepath = std::string(args[0]);
                parser.set();
            } else {
                cerr << "'open' command requires an absolute filepath argument." << endl;
            }
            return {};
        }
    };
    
    message<> reset { this, "reset", "Reset the transport",
        MIN_FUNCTION {
            this->trail = 0;
            return {};
        }
    };
                
    message<> play { this, "play", "Start playback.",
        MIN_FUNCTION {
            this->playing = true;
            this->trail = 0;
            for (auto i=0; i<8; ++i) {
                if (instruments[i].active)
                    instruments[i].enabled = true;
            }
            return {};
        }
    };
                
                message<> stop { this, "stop", "Stop playback.",
                    MIN_FUNCTION {
                        this->playing = false;
                        return {};
                    }
                };
                
                
    long mstos(double ms) {
        return ms * samplerate() / 1000.0;
    }
    double stoms(long s) {
        return s / samplerate() * 1000.0;
    }
    void negative_clear(audio_bundle output, short index) {
        auto out = output.samples(index);
        for (auto s=0; s<output.frame_count(); ++s)
            out[s] = 0.0;
    }

    void operator()(audio_bundle input, audio_bundle output) {
        double dub_frame = (double) output.frame_count();
        if (!this->playing) {
            output.clear();
        } else if (this->is_loaded) {
            for (auto channel=0; channel<output.channel_count(); ++channel) {
                instrument* inst = &instruments[channel];
                if (!inst->enabled) {
                    negative_clear(output, channel);
                } else {
                    auto out = output.samples(channel);
                    if (inst->cache == NULL) {
                        measure* meas = inst->m_first;
                        inst->prev = 0;
                        while ((meas != NULL) && (mstos(meas->offset) < trail)) {
                            inst->prev = trail;
                            meas = meas->next;
                        }
                        if (meas == NULL) {
                            inst->prev = trail;
                            meas = inst->m_first;
                        }
                        inst->cache = meas;
                        info_out->send(channel, 1);
                    }
                    // send information about next measure
                    //info_out->send(channel, inst->cache->timesig);
                    long offset = mstos(inst->cache->offset);
                    // frame_start keeps track of the absolute track position relative
                    // to the active measure.
                    int frame_start = trail - offset;
                    
                    // these next two handle measure tacets
                    long wait_frame_base = offset - inst->prev;
                    long wait_frame = trail - inst->prev;
                    if (frame_start + dub_frame < 0) {
                        for (auto s=0; s<dub_frame; ++s) {
                            // waiting...
                            out[s] = ((double) (wait_frame + s))/wait_frame_base;
                        }
                    } else {
                        for (auto s=0; s<dub_frame; ++s) {
                            int target_frame = frame_start + s;
                            // waiting...
                            if (target_frame < 0) {
                                out[s] = ((double) (wait_frame + s))/wait_frame_base;
                            } else if (target_frame >= inst->cache->bank.size()) {
                                if (inst->cache->next == NULL) {
                                    inst->cache = NULL;
                                    inst->enabled = false;
                                    break;
                                }
                                inst->prev = trail + s;
                                inst->cache = inst->cache->next;
                                wait_frame_base = offset - inst->prev;
                                wait_frame = trail - inst->prev;
                                offset = mstos(inst->cache->offset);
                                
                                long gap = offset - trail - s;
                                cout << inst->cache->offset << endl;
                                cout << trail << endl;
                                cout << trail + s << endl;
                                info_out->send(channel, "gap", (int) gap);
                                if (gap >= 0) {
                                    // send break information
                                    info_out->send(channel, 1, "tacet");
                                } else {
                                    // send information about next measure
                                    info_out->send(channel, inst->cache->timesig, "next");
                                }
                                
                                
                                frame_start = trail - offset;// + s;
                            } else {
                                if (target_frame == 0)
                                    info_out->send(channel, inst->cache->timesig, "first");
                                out[s] = inst->cache->bank[target_frame];
                            }
                        }
                    }
                }
            }
            trail += dub_frame;
            /*for (auto s=0; s<dub_frame; ++s) {
                if (trail + s > meas->bank.size())
                    trail = 0;
                out[s] = in[s] + meas->bank[(trail + s) % meas->bank.size()];
            }
            
            if (trail >= meas->bank.size())
                trail = trail % meas->bank.size();
*/
        } else {
            output.clear();
        }
        
        
		/*if (x != 0.0 && prev == 0.0)
			output_true.send(k_sym_bang);	// change from zero to non-zero
		else if (x == 0.0 && prev != 0.0)
			output_false.send(k_sym_bang);	// change from non-zero to zero
		prev = x;*/
	}
            
    void measure_calc(measure* meas) {
        
        // first, how many samples is this gonna be?
        // each measure will be subdivided into PPQ_tempo * timesig
        int subdiv { PPQ_tempo * meas->timesig };
        std::vector<int> lengths;
        double increment { (meas->end - meas->start)/subdiv };
        double height { 1.0/subdiv };
        int s;
        double sr = samplerate() * 60.0;
        for (s=0; s<subdiv; s++) {
            double bpm { meas->start + increment * s };
            int segment = sr/(PPQ_tempo*bpm);
            double inc { height/segment };
            for (int samp=0; samp<segment; samp++) {
                meas->bank.push_back((sample) (inc*samp + height*s));
            }
        }
    }

private:
                unique_ptr<inlet<>>             input;
    vector< unique_ptr<outlet<>> >   m_outlets;
    unique_ptr<outlet<thread_check::scheduler, thread_action::fifo>>    info_out;
                std::string filepath { "" };
                int m_channels { 1 };
	//sample            prev    { 0.0 };
    int               PPQ     { 0 };
    int               PPQ_tempo { 0 };
    bool              is_loaded { false };
                    bool playing { false };
    long trail {0};
    instrument instruments[8];
    
    //c74::max::t_symbol  fs      { "" };
    //c74::max::t_handle f_fh {};
    //char               filepath[MAX_PATH_CHARS];
    //bool        f_open { false };
};
    


MIN_EXTERNAL(bandit);
