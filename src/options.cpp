#include "options.hpp"

void Options::inputOptions(const int argc, const char* argv[]){
    stringstream ss;
    for (unsigned i=1; i<argc; ++i) {
        if (!strcmp(argv[i], "--inp-file")) {
            optionFile = argv[++i];
            readFile(optionFile);
            return;
        } else {
            if (i==1) ss << "\nOptions:\n\n";
        }
        if (!strcmp(argv[i], "--bayes")) {
            analysisType = "RAMBayes";
            bayesType = argv[++i];
            ss << "--bayes " << argv[i] << "\n";
        }
        else if (!strcmp(argv[i], "--ppbayes")) {
            analysisType = "PPBayes";
            bayesType = argv[++i];
            ss << "--ppbayes " << argv[i] << "\n";
        }
        else if (!strcmp(argv[i], "--ppasyncbayes")) {
            analysisType = "PPAsyncBayes";
            bayesType = argv[++i];
            ss << "--ppasyncbayes " << argv[i] << "\n";
        }
        else if (!strcmp(argv[i], "--preprocess")) {
            analysisType = "Preprocess";
            ss << "--preprocess " << "\n";
        }
        else if (!strcmp(argv[i], "--compress")) {
            compress = true;
            ss << "--compress " << "\n";
        }
        else if (!strcmp(argv[i], "--bfile")) {
            bedFile = argv[++i];
            ss << "--bfile " << argv[i] << "\n";
        }
        else if (!strcmp(argv[i], "--pheno")) {
            phenotypeFile = argv[++i];
            ss << "--pheno " << argv[i] << "\n";
        }
        else if (!strcmp(argv[i], "--mcmc-samples")) {
            mcmcSampleFile = argv[++i];
            ss << "--mcmc-samples " << argv[i] << "\n";
        }
        else if (!strcmp(argv[i], "--chain-length")) {
            chainLength = atoi(argv[++i]);
            ss << "--chain-length " << argv[i] << "\n";
        }
        else if (!strcmp(argv[i], "--burn-in")) {
            burnin = atoi(argv[++i]);
            ss << "--burn-in " << argv[i] << "\n";
        }
        else if (!strcmp(argv[i], "--seed")) {
            seed = atoi(argv[++i]);
            ss << "--seed " << argv[i] << "\n";
        }
        else if (!strcmp(argv[i], "--out")) {
            title = argv[++i];
            ss << "--out " << argv[i] << "\n";
        }
        else if (!strcmp(argv[i], "--thin")) {
            thin = atoi(argv[++i]);
            ss << "--thin " << argv[i] << "\n";
        }
        else if (!strcmp(argv[i], "--S")) {
            Gadget::Tokenizer strvec;
            strvec.getTokens(argv[++i], " ,");
            S.resize(strvec.size());
            for (unsigned j=0; j<strvec.size(); ++j) {
                S[j] = stof(strvec[j]);
            }
            ss << "--S " << argv[i] << "\n";
        }
        //Daniel, include variance components matrix for groups
        else if (!strcmp(argv[i], "--mS")) {
            Gadget::Tokenizer strvec;
            Gadget::Tokenizer strT;
            strvec.getTokens(argv[++i], " ;");
            strT.getTokens(strvec[0],",");
            mS=Eigen::MatrixXd(strvec.size(),strT.size());
            numGroups=strvec.size();
            for (unsigned j=0; j<strvec.size(); ++j) {
                strT.getTokens(strvec[j],",");
                for(unsigned k=0;k<strT.size();++k)
                    mS(j,k) = stod(strT[k]);
            }
            ss << "--mS " << argv[i] << "\n";
        }
        //Daniel group assignment file
        else if (!strcmp(argv[i], "--group")) {
            groupFile = argv[++i];
            ss << "--group " << argv[i] << "\n";
        }
        else if (!strcmp(argv[i], "--thread")) {
            numThread = atoi(argv[++i]);
            ss << "--thread " << argv[i] << "\n";
        }
        else if(!strcmp(argv[i], "--thread-spawned")) {
            numThreadSpawned = atoi(argv[++i]);
            ss << "--thread-spawned " << argv[i] << "\n";
        }
        else {
            stringstream errmsg;
            errmsg << "\nError: invalid option \"" << argv[i] << "\".\n";
            throw (errmsg.str());
        }
    }

    cout << ss.str() << endl;
}

void Options::readFile(const string &file){  // input options from file
    optionFile = file;
    stringstream ss;
    ss << "\nOptions:\n\n";
    ss << boost::format("%20s %-1s %-20s\n") %"optionFile" %":" %file;
    makeTitle();

    ifstream in(file.c_str());
    if (!in) throw ("Error: can not open the file [" + file + "] to read.");

    string key, value;
    while (in >> key >> value) {
        if (key == "bedFile") {
            bedFile = value;
        } else if (key == "phenotypeFile") {
            phenotypeFile = value;
        } else if (key == "bedFile") {
            bedFile = value;
        } else if (key == "analysisType") {
            analysisType = value;
        } else if (key == "bayesType") {
            bayesType = value;
        } else if (key == "mcmcSampleFile") {
            mcmcSampleFile = value;
        } else if (key == "chainLength") {
            chainLength = stoi(value);
        } else if (key == "burnin") {
            burnin = stoi(value);
        } else if (key == "seed") {
            seed = stoi(value);
        } else if (key == "thin") {
            thin = stoi(value);
        } else if (key == "S") {
            Gadget::Tokenizer strvec;
            strvec.getTokens(value, " ,");
            S.resize(strvec.size());
            for (unsigned j=0; j<strvec.size(); ++j) {
                S[j] = stof(strvec[j]);
            }
        } else if (key == "numThread") {
            numThread = stoi(value);
        } else if (key.substr(0,2) == "//" ||
                key.substr(0,1) == "#") {
            continue;
        } else {
            throw("\nError: invalid option " + key + " " + value + "\n");
        }
        ss << boost::format("%20s %-1s %-20s\n") %key %":" %value;
    }
    in.close();

    cout << ss.str() << endl;



}

void Options::makeTitle(void){
    title = optionFile;
    size_t pos = optionFile.rfind('.');
    if (pos != string::npos) {
        title = optionFile.substr(0,pos);
    }
}
