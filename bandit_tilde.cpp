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
} instrument;



class bandit : public object<bandit>, public vector_operator<> {
public:

	MIN_DESCRIPTION { "Polytempo timing object for the Bandit ecosystem." };
	MIN_TAGS		{ "audio" };
	MIN_AUTHOR		{ "J. Kusel" };
	MIN_RELATED		{ "" };
    
    inlet<>                                                 sig             { this, "(signal)", "signal" };
	inlet<>													input			{ this, "(open) bandit" };
    outlet<>                                                outsig          { this, "(signal)", "signal" };
	outlet<thread_check::scheduler, thread_action::fifo>	output_true		{ this, "(bang) input is non-zero" };
	outlet<thread_check::scheduler, thread_action::fifo>	output_false	{ this, "(bang) input is zero" };
    
    // deferred file loading
    queue<> deferrer { this,
        MIN_FUNCTION {
            std::ifstream in { this->filepath };
            short line_count { -2 };
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
                arguments.copy(this->filepath, arguments.size(), 0);
            
                cout << args.size() << endl;
                cout << arguments << endl;
                deferrer.set();
            } else {
                cerr << "'open' command requires an absolute filepath argument." << endl;
            }
            return {};
        }
    };

    void operator()(audio_bundle input, audio_bundle output) {
        if (this->is_loaded) {
            //output = input;
            //std::vector<sample> chunk = std::vector<sample>(output.size(), (sample) 0.0);
            measure* meas = instruments[0].m_first;
            auto out = output.samples(0);
            auto in = input.samples(0);
            double dub_frame = (double) input.frame_count();
            if (meas->bank.empty()) {
                for (auto s=0; s<dub_frame; ++s) {
                    out[s] = 5.0;//(sample) meas->bank.operator[](0);//meas->bank->operator[](100);//meas->bank->tail((size_t) (s+instruments[0].tail));
                }
            } else {
            for (auto s=0; s<dub_frame; ++s) {
                if (trail + s > meas->bank.size())
                    trail = 0;
                out[s] = in[s] + meas->bank[(trail + s) % meas->bank.size()];//(sample) meas->bank.operator[](0);//meas->bank->operator[](100);//meas->bank->tail((size_t) (s+instruments[0].tail));
            }
            }
            trail += input.frame_count();
            if (trail >= meas->bank.size())
                trail = trail % meas->bank.size();
            //instruments[0].tail = (instruments[0].tail + output.frame_count()) % meas->bank->size();

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
        //cout << subdiv << " " << increment << endl;
        int s;
        //int total { 0 };
        // assumes 44.1kHz sample rate
        
        for (s=0; s<subdiv; s++) {
            double bpm { meas->start + increment * s };
            int segment = 2646000.0/(PPQ_tempo*bpm);
            double inc { height/segment };
            for (int samp=0; samp<segment; samp++) {
                meas->bank.push_back((sample) (inc*samp + height*s));
            }
        }
    }

private:
	//sample            prev    { 0.0 };
    int               PPQ     { 0 };
    int               PPQ_tempo { 0 };
            bool              is_loaded { false };
            int trail {0};
    instrument instruments[8];
    
    //c74::max::t_symbol  fs      { "" };
    //c74::max::t_handle f_fh {};
    char               filepath[MAX_PATH_CHARS];
    //bool        f_open { false };
};
    


MIN_EXTERNAL(bandit);
