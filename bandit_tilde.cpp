/// @file
///	@ingroup 	minexamples
///	@copyright	Copyright 2018 The Min-DevKit Authors. All rights reserved.
///	@license	Use of this source code is governed by the MIT License found in the License.md file.

#include "c74_min.h"

using namespace c74::min;


class bandit : public object<bandit>, public sample_operator<1,0> {
public:

	MIN_DESCRIPTION { "Polytempo timing object for the Bandit ecosystem." };
	MIN_TAGS		{ "audio" };
	MIN_AUTHOR		{ "J. Kusel" };
	MIN_RELATED		{ "" };

	inlet<>													input			{ this, "(open) bandito" };
	outlet<thread_check::scheduler, thread_action::fifo>	output_true		{ this, "(bang) input is non-zero" };
	outlet<thread_check::scheduler, thread_action::fifo>	output_false	{ this, "(bang) input is zero" };
    
    // deferred file loading
    queue<> deferrer { this,
        MIN_FUNCTION {
            /*short fpath;
            path filepath = path({ args });
            char ps[MAX_PATH_CHARS];
            char ps2[MAX_PATH_CHARS];
            char fullps[MAX_PATH_CHARS];
//#ifdef FILEBYTE_WINDOWS_SPECIFIC
            //char	 	ps2[MAX_PATH_CHARS];
//#endif
            c74::max::t_fourcc type;
             */
            //atom asdf { "hello" };
            //path filepath = path({ args });
            
            
            std::ifstream in { this->filepath };
            for (std::string line; std::getline(in, line); )
            {
                size_t pos { 0 };
                string token;
                short col = 0;
                while ((pos = line.find(",")) != std::string::npos) {
                    token = line.substr(0, pos);
                    
                    cout << col << token << endl;
                    col++;
                    line.erase(0, pos+1);
                }
                //cout << line << endl;
            }
            /*string content;
            content = string { std::istreambuf_iterator<char>(in),
                std::istreambuf_iterator<char>() };
            cout << content << endl;*/
            //c74::max::t_max_err err;
            //from_atoms<string>(args);

            /*if (filepath.name() == "") {
                
                if (c74::max::open_dialog(ps, &fpath, &type, NULL, 0))
                    return {};
                try {
                    c74::max::path_topathname(fpath,ps,fullps);
#ifdef WIN_VERSION
                    c74::max::path_nameconform(ps2,ps,PATH_STYLE_NATIVE_WIN, 1);
#else
                    c74::max::path_nameconform(ps2,ps,c74::max::PATH_STYLE_SLASH, 1);
#endif
                    
                    std::ifstream   in  { ps2 };
                    string test_content;
                    test_content = string { std::istreambuf_iterator<char>(in),
                        std::istreambuf_iterator<char>() };
                    cout << test_content << endl;
                } catch (...) {
                    cerr << "Error locating file." << endl;
                }
            } else {
                filepath.name().copy(fullps, filepath.name().size(), 0);
                if (c74::max::locatefile_extended(ps, &fpath, &type, &type, 0)) {
                    cerr << "File not found." << endl;
                    return {};
                }
            }
             */
            
                    
            
                    
#ifdef WIN_VERSION
            HMODULE hm = nullptr;
            // taking this from min.project.cpp, who knows what's happening here
#endif
                    
                    
                    
            //cout << ps2 << endl;	// post to the max console
            return {};
            
            
        }
    };
        
    // respond to the bang message to do something
    message<> open { this, "open", "Open a file.",
        MIN_FUNCTION {
            
            string arguments = to_string(args);
            if (args.size() == 1) {
                arguments.copy(this->filepath, arguments.size(), 0);
            
                cout << args.size() << endl;
                cout << arguments << endl;
                deferrer.set();
            }
            return {};
        }
    };

	void operator()(sample x) {
		if (x != 0.0 && prev == 0.0)
			output_true.send(k_sym_bang);	// change from zero to non-zero
		else if (x == 0.0 && prev != 0.0)
			output_false.send(k_sym_bang);	// change from non-zero to zero
		prev = x;
	}

private:
	sample              prev    { 0.0 };
    //c74::max::t_symbol  fs      { "" };
    //c74::max::t_handle f_fh {};
    char               filepath[MAX_PATH_CHARS];
    //bool        f_open { false };
};

MIN_EXTERNAL(bandit);
