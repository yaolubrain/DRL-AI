#include "DeepQAgent.hpp"
#include "random_tools.h"
#include <iostream>
#include <zlib.h>
#include <iterator>
#include <cstring>
#include "../dnq/nn.h"
#include "../dnq/layer.h"
#include "../dnq/parameter.h"


DeepQAgent::DeepQAgent(OSystem* _osystem, RomSettings* _settings) :
    PlayerAgent(_osystem, _settings) {

    process_count_ = 0;
    sampling_array_ = new double[4*SAMPLE_WIDTH*SAMPLE_HEIGHT];

}
DeepQAgent::~DeepQAgent()
{
    delete[] sampling_array_;
}
Action DeepQAgent::act() {
    Action a = Action(2);//legal_actions[rand() % legal_actions.size()];
    //return choice(&available_actions);
    return a;
    //return choice(&available_actions);
}

void DeepQAgent::SavePNG(IntMatrix* screen_matrix)
{
    p_osystem->p_export_screen->export_any_matrix(screen_matrix,"check.png");
    return;
}

void DeepQAgent::SaveGrayPNG(double** gray_matrix, int width, int height)
{
    uInt8* buffer  = (uInt8*) NULL;
    uInt8* compmem = (uInt8*) NULL;
    ofstream out;

    try {
        // Get actual image dimensions. which are not always the same
        // as the framebuffer dimensions
        out.open("check_gray.png", ios_base::binary);
        if(!out)
            throw "Couldn't open PNG file";

        // PNG file header
        uInt8 header[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
        out.write((const char*)header, 8);

        // PNG IHDR
        uInt8 ihdr[13];
        ihdr[0]  = width >> 24;   // i_screen_width
        ihdr[1]  = width >> 16;
        ihdr[2]  = width >> 8;
        ihdr[3]  = width & 0xFF;
        ihdr[4]  = height >> 24;  // i_screen_height
        ihdr[5]  = height >> 16;
        ihdr[6]  = height >> 8;
        ihdr[7]  = height & 0xFF;
        ihdr[8]  = 8;  // 8 bits per sample (24 bits per pixel)
        ihdr[9]  = 2;  // PNG_COLOR_TYPE_RGB
        ihdr[10] = 0;  // PNG_COMPRESSION_TYPE_DEFAULT
        ihdr[11] = 0;  // PNG_FILTER_TYPE_DEFAULT
        ihdr[12] = 0;  // PNG_INTERLACE_NONE
        p_osystem->p_export_screen->writePNGChunk(out, "IHDR", ihdr, 13);

        // Fill the buffer with scanline data
        int rowbytes = width * 3;
        buffer = new uInt8[(rowbytes + 1) * height];
        uInt8* buf_ptr = buffer;
        for(int i = 0; i < height; i++) {
            *buf_ptr++ = 0;                  // first byte of row is filter type
            for(int j = 0; j < width; j++) {
                int gray_value = gray_matrix[i][j];
                buf_ptr[j * 3 + 0] = gray_value;
                buf_ptr[j * 3 + 1] = gray_value;
                buf_ptr[j * 3 + 2] = gray_value;
            }
            buf_ptr += rowbytes;                 // add pitch
        }

        // Compress the data with zlib
        uLongf compmemsize = (uLongf)((height * (width + 1) * 3 * 1.001 + 1) + 12);
        compmem = new uInt8[compmemsize];
        if(compmem == NULL ||
                (compress(compmem, &compmemsize, buffer, height *
                          (width * 3 + 1)) != Z_OK))
            throw "Error: Couldn't compress PNG";

        // Write the compressed framebuffer data
        p_osystem->p_export_screen->writePNGChunk(out, "IDAT", compmem, compmemsize);

        // Add some info about this snapshot
        p_osystem->p_export_screen->writePNGText(out, "ROM Name", p_osystem->p_export_screen->p_props->get(Cartridge_Name));
        p_osystem->p_export_screen->writePNGText(out, "ROM MD5", p_osystem->p_export_screen->p_props->get(Cartridge_MD5));
        p_osystem->p_export_screen->writePNGText(out, "Display Format", p_osystem->p_export_screen->p_props->get(Display_Format));

        // Finish up
        p_osystem->p_export_screen->writePNGChunk(out, "IEND", 0, 0);

        // Clean up
        if(buffer)  delete[] buffer;
        if(compmem) delete[] compmem;
        out.close();

    }
    catch(const char *msg)
    {
        if(buffer)  delete[] buffer;
        if(compmem) delete[] compmem;
        out.close();
        cerr << msg << endl;
    }

}
void DeepQAgent::DownSampling(double **gray_array,double **sample_array,int width_size, \
                              int height_size, int sample_width, int sample_height){
    double scale_height = (double)sample_height / (double)height_size;
    double scale_width =  (double)sample_width / (double)width_size;

    for(int cy = 0; cy < sample_height; ++cy){
        for(int cx = 0; cx < sample_width; ++cx){
            sample_array[cy][cx] = gray_array[(int)(cy / scale_height)][(int)(cx/scale_width)];
        }
    }
}




void DeepQAgent::RGB2Gray(double** gray_array, uInt8 *buffer, int width, int height){
    uInt8* buf_ptr = buffer;
    int rowbytes = width * 3;
    for(int i = 0; i < height; ++i){
        for(int j = 0; j < width; ++j){
            gray_array[i][j] = 0.2989 * buf_ptr[j * 3 + 0] + 0.5879 * buf_ptr[j * 3 + 1] + 0.1140 * buf_ptr[j * 3 + 2];
        }
        buf_ptr += rowbytes+1; //added one so that the gray picture is not distorted.
    }
}


Action DeepQAgent::agent_step() {
    // Terminate if we have a maximum number of frames
    if (i_max_num_frames > 0 && frame_number >= i_max_num_frames)
        end_game();

    // Terminate this episode if we have reached the max. number of frames
    if (i_max_num_frames_per_episode > 0 &&
            episode_frame_number >= i_max_num_frames_per_episode) {
        return RESET;
    }
    // Only take an action if manual control is disabled
    Action a;
    a = act();
    int sample_width_;
    int sample_height_;

    sample_width_= 100;
    sample_height_=100;

    int screen_width_;
    int screen_height_;

    screen_width_ = p_osystem->console().mediaSource().width();
    screen_height_ = p_osystem->console().mediaSource().height();

    IntMatrix screen_matrix;
    for (int i=0; i<screen_height_; ++i) {
        IntVect row;
        for (int j=0; j<screen_width_; ++j)
            row.push_back(-1);
        screen_matrix.push_back(row);
    }
    uInt8* pi_curr_frame_buffer = p_osystem->console().mediaSource().currentFrameBuffer();
    int ind_i, ind_j;
    for (int i = 0; i < screen_width_ * screen_height_; i++) {
        uInt8 v = pi_curr_frame_buffer[i];
        ind_i = i / screen_width_;
        ind_j = i - (ind_i * screen_width_);
        screen_matrix[ind_i][ind_j] = v;
    }

    // Process the Matrix
    uInt8* buffer  = (uInt8*) NULL;
    int rowbytes = screen_width_ * 3;
    buffer = new uInt8[(rowbytes + 1) * screen_height_];
    uInt8* buf_ptr = buffer;
    for(int i = 0; i < screen_height_; i++) {
        *buf_ptr++ = 0;                  // first byte of row is filter type
        for(int j = 0; j < screen_width_; j++) {
            int r, g, b;
            p_osystem->p_export_screen->get_rgb_from_palette((screen_matrix)[i][j], r, g, b);
            buf_ptr[j * 3 + 0] = r;
            buf_ptr[j * 3 + 1] = g;
            buf_ptr[j * 3 + 2] = b;
        }
        buf_ptr += rowbytes;                 // add pitch
    }

    double ** gray_buffer_ = NULL;
    double ** sample_buffer_ = NULL;

    gray_buffer_ = CreateArray(screen_width_,screen_height_);
    sample_buffer_ = CreateArray(sample_width_,sample_height_);

    // Gray scaling
    this->RGB2Gray(gray_buffer_,buffer,screen_width_,screen_height_);

    // DownSampling Reduce Size
    this->DownSampling(gray_buffer_,sample_buffer_,screen_width_,screen_height_,\
                       sample_width_,sample_height_);
    //cout<< this->p_rom_settings->getReward()<<endl;


    // Save screen_matrix for Debugging
    //this->SavePNG(&screen_matrix);

    //this->SaveGrayPNG(gray_buffer_,screen_width_,screen_height_);




    // Copy sample file to the perminant address
    for(int i = 0; i < SAMPLE_WIDTH ; i++)
    {
        for(int j = 0; j < SAMPLE_HEIGHT ; j++)
        {
            sampling_array_[process_count_*SAMPLE_WIDTH*SAMPLE_HEIGHT+ i*j] = sample_buffer_[i][j];
        }
    }

    /*************************************************************/


    NNParameter* nn_para = new NNParameter;

     nn_para->im_size_ = 100;
     nn_para->data_batch_num_ = 5;
     nn_para->data_batch_size_ = 10000;
     nn_para->sample_size_ = nn_para->data_batch_num_ * nn_para->data_batch_size_;
     nn_para->channel_num_ = 3;
     nn_para->class_num_ = 2;

     nn_para->learn_rate_ = 0.001;
     nn_para->mb_size_ = 50;
     nn_para->epoch_num_ = 10;

     // layer parameters
     LayerParameter* conv1 = new LayerParameter();
     LayerParameter* pool1 = new LayerParameter();
     LayerParameter* conv2 = new LayerParameter();
     LayerParameter* pool2 = new LayerParameter();
     LayerParameter* full1 = new LayerParameter();

     conv1->type_ = "conv";
     conv1->input_im_size_ = nn_para->im_size_;
     conv1->input_size_ = (conv1->input_im_size_)*(conv1->input_im_size_);
     conv1->input_num_ = 4;
     conv1->filter_size_ = 5;
     conv1->filter_num_ = 20;

     pool1->type_ = "pool";
     pool1->pool_dim_ = 2;

     conv2->type_ = "conv";
     conv2->filter_size_ = 5;
     conv2->filter_num_ = 20;

     pool2->type_ = "pool";
     pool2->pool_dim_ = 2;

     full1->type_ = "full";
     full1->output_size_ = nn_para->class_num_;

     nn_para->layer_para_.push_back(conv1);
     nn_para->layer_para_.push_back(pool1);
     nn_para->layer_para_.push_back(conv2);
     nn_para->layer_para_.push_back(pool2);
     nn_para->layer_para_.push_back(full1);

     NN* nn = new NN(nn_para);

//     nn->Init();
//     nn->Train();

     delete nn;


    /*************************************************************/


    // Destroy Gray Array
    DestroyImgBuf(gray_buffer_,screen_height_);

    // Destory Sample Array
    DestroyImgBuf(sample_buffer_,sample_height_);

    // Clean up
    if(buffer)  delete[] buffer;


    if( 3 == process_count_){
        process_count_ = -1;
        // Save Sample for Debugging
        //this->SaveGrayPNG(sampling_array_[0][0],sample_width_,sample_height_);
        //std::cout << process_count_ << endl;
    }

    if (record_trajectory) record_action(a);

    process_count_ ++;
    frame_number++;
    episode_frame_number++;

    return a;
}
