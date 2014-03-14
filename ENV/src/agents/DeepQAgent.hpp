#ifndef DEEP_Q_AGENT_H
#define DEEP_Q_AGENT_H

#include "../common/Constants.h"
#include "PlayerAgent.hpp"
#include "../emucore/OSystem.hxx"


inline double** CreateArray(int width, int height)
{
    double** array   = new double*[height];
    for(int i = 0; i < height; ++i)
        array[i] = new double[width];
    return array;
}
inline void DestroyImgBuf(double ** &array,int height)
{
    for(int i = 0; i < height; ++i)
        delete[] array[i];
    delete[] array;
    array = NULL;
}
class DeepQAgent : public PlayerAgent
{
public:

    DeepQAgent(OSystem* _osystem,RomSettings* _settings);
    ~DeepQAgent();




protected:
    /* *********************************************************************
            Returns the best action from the set of possible actions
         ******************************************************************** */
    virtual Action act();
    virtual Action agent_step();
private:


    void DownSampling(double **gray_array,double ** sample_arrayint, int width_size, int height_size, int sample_width, int sample_height);
    void SavePNG(IntMatrix* const screen_matrix);
    void SaveGrayPNG(double** gray_buffer, int sample_width, int sample_height);
    void RGB2Gray(double** gray_array, uInt8 *buffer, int width, int height);



};

#endif // DEEP_Q_AGENT_H
