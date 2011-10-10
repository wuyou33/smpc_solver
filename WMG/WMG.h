/**
 * @file
 * @author Alexander Sherikov
 * @date 27.09.2011 18:57:37 MSD
 */


#ifndef WMG_H
#define WMG_H


/****************************************
 * INCLUDES 
 ****************************************/

#include <string>
#include <vector>

#include "common_const.h"


/****************************************
 * TYPEDEFS 
 ****************************************/
class FootStep;


/// @addtogroup gWMG_API
/// @{

enum WMGret
{
    WMG_OK,
    WMG_HALT
};

enum fs_type
{
    FS_TYPE_SS,
    FS_TYPE_DS
};

/** \brief Defines the parameters of the Walking Pattern Generator. */
class WMG
{
    public:
        WMG();
        ~WMG();
        void init (const int, const double, const double);
        void AddFootstep(
                const double, 
                const double, 
                const double, 
                const int, 
                const int, 
                const double *, 
                const fs_type type = FS_TYPE_SS);
        void AddFootstep(
                const double, 
                const double, 
                const double, 
                const int, 
                const int, 
                const fs_type type = FS_TYPE_SS);
        void AddFootstep(
                const double, 
                const double, 
                const double, 
                const fs_type type = FS_TYPE_SS);
        WMGret FormPreviewWindow();
        void FS2file(const std::string);



        /** \brief A vector of footsteps. */
        std::vector<FootStep> FS; 


        /** \brief Number of iterations in a preview window. */
        int N;

        /** \brief Preview sampling time  */
        double *T;


        /** \brief Height of the CoM. */
        double hCoM;

        /** \brief Norm of the acceleration due to gravity. For the moment gravity is set equal to 9.81. */
        double gravity;
        
        /** \brief h = #hCoM/#gravity. */
        double *h;
        
        /// A chunk of memory allocated for solution.
        double *X;

        /** \brief Initial state. */
        double X_tilde[NUM_STATE_VAR];

        double *angle;
        double *zref_x;
        double *zref_y;
        double *lb;
        double *ub;

    private:
        double def_constraint[4];
        int def_repeat_times;

        double def_ds_constraint[4];
        int def_ds_num;

        /** \brief This is the step in FS that is at the start of the current preview window. I don't need
        to pop_front steps from FS and that is why I use stl vector and not stl deque). */
        int current_step_number;
};

///@}

#endif /*WMG_H*/