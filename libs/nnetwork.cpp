#include "nnetwork.h"
#include "img_process.h"

const unsigned int num_input = resx * resy;
const unsigned int num_output = 4;
const float learn_rate = 0.1;
const float bit_fail_limit = 0.01f;

unsigned int max_epochs = 1000;
float desired_error = 0.001f;
unsigned int epochs_between_reports = 50;

// void nn::setVars(unsigned int me, float de, unsigned int ebr){
// 	max_epochs = me;
// 	desired_error = de;
// 	epochs_between_reports = ebr;
// }

using namespace nn;

class TrainData{
	public:
		vector<float> input;
		float output[num_output];
			/*
			posx
			posy
			width
			height
			*/
		TrainData(Mat mat, Object obj, int img_width, int img_height){
			
			#if DEBUG
			cout << "Image width: " << img_width << endl;
			cout << "Posx: " << obj.x << endl;
			#endif
			
			input = getInputs(mat);
			
			output[0] = (float)obj.x / img_width;
			output[1] = (float)obj.y / img_height;
			output[2] = (float)obj.w / img_width;
			output[3] = (float)obj.h / img_height;
			
			#if DEBUG
			cout << "Input vector is " << input.size() << " long" << endl;
			cout << "Output array is " << num_output << " long" << endl;
			#endif
		};
		static vector<float> getInputs(Mat mat){
			vector<float> out;
			vector<uchar> array;array.reserve(num_input);
			array.assign(mat.datastart, mat.dataend);
			for(auto it = array.begin(); it != array.end(); ++it){
				out.push_back (((float)(*it)) / 255);
			}
			return out;
		}
		
};

// * Importing *****************************************
int import_count = 0;
bool tried_count = false;
void getCountFile(fs::path cacheDir){
	fs::path countFPath ((cacheDir.string() + "/cache.txt").c_str());
	//Getting count file
	if(!tried_count){
		if(fs::exists(countFPath)){
			cout << "Reading count from " << countFPath << endl;
			ifstream countf(countFPath.c_str());
			if(countf.is_open()){
				string line;
				getline(countf, line);
				import_count = getInt(line);
				countf.close();
			}
		}
		tried_count = true;
	}
}
void nn::setCountFile(fs::path cacheDir){
	fs::path countFPath ((cacheDir.string() + "/cache.txt").c_str());
	cout << "Writing count to " << countFPath << endl;
	
	ofstream countf(countFPath.c_str());
	if(countf.is_open()){
		countf << import_count << endl;
		countf.close();
	}else
		cout << "Unable to write to counter file in cache :/" << endl;
}

#include <fstream>

void nn::importFile(fs::path cacheDir, fs::path _path, vector<Object> objects){
	getCountFile(cacheDir);
	
	auto mat = cv::imread (_path.string());
	
	cv::Mat trainMat;
	
	trainMat = ip::getTrainMat(mat);
	
	//This function will copy the image to a new location
	cv::imshow("Importing..", writeObjects(mat, objects));
	// #if DEBUG
	// waitKey(30);
	// #else
	waitKey(10);
	// #endif
	
	//Saving file to cache
	imwrite((cacheDir.string() + "/" + Object::getString(objects) + "-" + to_string(import_count++) + ".jpg").c_str(), trainMat);
}
// * --------------------------------------------

vector<TrainData> train_data;

void nn::readFile(fs::path filePath){
	// cout << "Reading " << filePath << endl;
	auto matRaw = cv::imread (filePath.string());
	// cout << "Resizing" << endl;
	Mat mat;resize(matRaw, mat, Size(resx, resy));
	// cout << "Pushing" << endl;
	train_data.push_back(TrainData(mat, Object::getObjects(filePath.filename().string())[0], matRaw.cols, matRaw.rows));
}

void nn::saveTrainData(fs::path savePath){
	ofstream dfile;
	dfile.open(savePath.c_str());
	dfile << train_data.size() << " " << num_input << " " << num_output << endl;
	for(auto it = train_data.begin(); it != train_data.end(); ++it){
		auto input = &(*it).input;
		auto output = (*it).output;
		for(auto dit = input->begin(); dit != input->end(); ++dit){
			if(dit != input->begin())dfile << " ";
			dfile << *dit;
		}
		dfile << endl;
		for(int i = 0; i < num_output; i++){
			if(i != 0)dfile << " ";
			dfile << output[i];
		}
		dfile << endl;
	}
	dfile.close();
}

struct fann* nn::ann_load(fs::path nn_path){
	if(!fs::exists(nn_path) || !fs::is_regular_file(nn_path)){cout << "either nn_path doesn't exist, or it is not a file" << endl;return NULL;}
	struct fann *ann = fann_create_from_file(nn_path.c_str());
	return ann;
}
struct fann *cube_ann, *sphere_ann;
void loadNNs(fs::path cube_nn_path, fs::path sphere_nn_path){
	cube_ann = ann_load(cube_nn_path);
	sphere_ann = ann_load(sphere_nn_path);
}
void freeNNs(){
	if(cube_ann)fann_destroy(cube_ann);
	if(sphere_ann)fann_destroy(sphere_ann);
}

Object execute(Mat mat, struct fann* ann, int actual_w, int actual_h){
	Object o;
	
	//Resizing image to correct input
	if(mat.size().area() != ann->num_input){
		if(resx * resy == ann->num_input){
			resize(mat, mat, Size(resx, resy));
		}else{
			cout << "Image size is not correct input for the network" << endl;
			return o;
		}
	}
	
	fann_type *calc_out;
	fann_type input[ann->num_input];
	
	//Set input
	auto inputs = TrainData::getInputs(mat);
	for(int i = 0;i<inputs.size();i++){
		input[i] = inputs[i];
	}
	
	calc_out = fann_run(ann, input);
	
	//Collect output
	cout << "Output: " 
	<< calc_out[0] * actual_w << "  "
	<< calc_out[1] * actual_h << "  "
	<< calc_out[2] * actual_w << "  "
	<< calc_out[3] * actual_h
	<< endl;
	o.x = calc_out[0] * actual_w;
	o.y = calc_out[1] * actual_h;
	o.w = calc_out[2] * actual_w;
	o.h = calc_out[3] * actual_h;
	
	return o;
}

Object nn::execute_test_cube_nn(Mat mat, int actual_w, int actual_h, fs::path cubeNNPath){
	auto ann = ann_load(cubeNNPath);
	Object o;
	if(ann){
		o = execute(mat, ann, actual_w, actual_h);
		fann_destroy(ann);
	}
	return o;
}
vector<Object> nn::execute(Mat mat){
	vector<Object> objs;
	if(!cube_ann || !sphere_ann){cout << "One of the fanns is null" << endl;return objs;}
	if(mat.empty()){cout << "Mat is empty" << endl;return objs;}
	if(mat.size().area() != cube_ann->num_input){cout << "Image size not equal cube nn input" << endl;return objs;}
	if(mat.size().area() != sphere_ann->num_input){cout << "Image size not equal sphere nn input" << endl;return objs;}
	//Now that everything is OK we can start
	
	
	return objs;
}

int train(struct fann* ann, fs::path dataPath, fs::path testPath){
	struct fann_train_data *data = fann_read_train_from_file(dataPath.c_str());
	struct fann_train_data *test_data = fann_read_train_from_file(testPath.c_str());
	int rcount = 0;
	for(int i = 1 ; i <= max_epochs ; i++) {
		auto error = fann_train_epoch(ann, data);
		auto b_fail = fann_get_bit_fail(ann);
		if ( error < desired_error || b_fail == 0 || checkSig() == false) {
			fann_reset_MSE(ann);
			fann_test_data(ann, test_data);
			auto t_error = fann_get_MSE(ann);
			auto t_b_fail = fann_get_bit_fail(ann);
			printf("EpocT: %d	Mean Square Error: %f	BitFail: %d\n", i, t_error, t_b_fail);
			if((t_error < desired_error && b_fail == 0) || checkSig() == false){
				fann_destroy_train(test_data);
				fann_destroy_train(data);
				return 1;
			}
		}
		
		rcount++;
		if(rcount == epochs_between_reports){
			rcount = 0;
			if(!testMat.empty()){
				auto pMat = ip::processMat(testMat, ip::colors[0]);
				cv::namedWindow("Final", WINDOW_NORMAL);
				cv::imshow("Final", writeObjects(testMat, execute(pMat, ann, testMat.cols, testMat.rows)));
				cv::updateWindow("Final");
			}
			printf("Epoch: %d	Mean Square Error: %f	BitFail: %d\n", i, error, fann_get_bit_fail(ann));
		}
	}
	fann_destroy_train(test_data);
	fann_destroy_train(data);
	return 0;
}
void nn::train(fs::path dataPath, fs::path testPath, fs::path nn_output){
	struct fann *ann = fann_create_standard(4, num_input, 70, 70, num_output);
	
	fann_set_activation_function_hidden(ann, FANN_SIGMOID_SYMMETRIC);
	fann_set_activation_function_output(ann, FANN_SIGMOID_SYMMETRIC);
	fann_set_bit_fail_limit(ann, bit_fail_limit);
	fann_set_learning_rate(ann, learn_rate);
	
	train(ann, dataPath, testPath);
	
	fann_save(ann, nn_output.c_str());
	fann_destroy(ann);
}
void nn::train(fs::path dataPath, fs::path testPath, fs::path nn_output, fs::path nn_file){
	struct fann *ann = fann_create_from_file(nn_file.c_str());
	
	train(ann, dataPath, testPath);
	
	fann_save(ann, nn_output.c_str());
	fann_destroy(ann);
}