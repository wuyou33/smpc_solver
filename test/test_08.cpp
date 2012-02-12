/** 
 * @file
 * @author Alexander Sherikov
 * @brief Simulate control loop, which is shorter than preview window iteration.
 */


#include <cstring> //strcmp
#include "tests_common.h"

///@addtogroup gTEST
///@{

int main(int argc, char **argv)
{
    //-----------------------------------------------------------
    // the numbers must correspond to the numbers in init_04()
    int control_sampling_time_ms = 20;
    int preview_sampling_time_ms = 40;
    int next_preview_len_ms = 0;

    // initialize
    WMG wmg;
    smpc_parameters par;
    init_07 (&wmg);
    par.init(wmg.N, wmg.hCoM/wmg.gravity);

    std::string fs_out_filename("test_08_fs.m");
    wmg.FS2file(fs_out_filename, false); // output results for later use in Matlab/Octave
    //-----------------------------------------------------------


    test_start(argv[0]);
    //-----------------------------------------------------------
    smpc::solver solver(
            wmg.N, // size of the preview window
            300.0,  // Alpha
            800.0,  // Beta
            1.0,    // Gamma
            0.01,   // regularization
            1e-7);  // tolerance
    solver.enable_fexceptions();
    //-----------------------------------------------------------



    //-----------------------------------------------------------
    wmg.initABMatrices ((double) control_sampling_time_ms / 1000);
    par.init_state.set (0.019978839010709938, -6.490507362468014e-05);
    // state_tilde = state_orig, when velocity = acceleration = 0
    wmg.X_tilde.set (0.019978839010709938, -6.490507362468014e-05);
    //-----------------------------------------------------------


    FILE *file_op = fopen(fs_out_filename.c_str(), "a");
    fprintf(file_op,"hold on\n");

    vector<double> ZMP_ref_x;
    vector<double> ZMP_ref_y;
    vector<double> ZMP_x;
    vector<double> ZMP_y;
    vector<double> CoM_x;
    vector<double> CoM_y;

    vector<double> left_foot_x;
    vector<double> left_foot_y;
    vector<double> left_foot_z;

    vector<double> right_foot_x;
    vector<double> right_foot_y;
    vector<double> right_foot_z;

    wmg.T_ms[0] = control_sampling_time_ms;
    wmg.T_ms[1] = control_sampling_time_ms;

    for(int i=0 ;; i++)
    {
        if (next_preview_len_ms == 0)
        {
            next_preview_len_ms = preview_sampling_time_ms;
        }   


        wmg.T_ms[2] = next_preview_len_ms;

        cout << wmg.isSupportSwitchNeeded() << endl;
        if (wmg.formPreviewWindow(par) == WMG_HALT)
        {
            cout << "EXIT (halt = 1)" << endl;
            break;
        }

        ZMP_ref_x.push_back(par.zref_x[0]);
        ZMP_ref_y.push_back(par.zref_y[0]);


        
        //------------------------------------------------------
        solver.set_parameters (par.T, par.h, par.h0, par.angle, par.zref_x, par.zref_y, par.lb, par.ub);
        solver.form_init_fp (par.fp_x, par.fp_y, par.init_state, par.X);
        solver.solve();
        //------------------------------------------------------
        // update state
        wmg.next_control.get_first_controls (solver);
        wmg.calculateNextState(wmg.next_control, par.init_state);
        //-----------------------------------------------------------


        if (next_preview_len_ms == preview_sampling_time_ms)
        {
            // if the values are saved on each iteration the plot becomes sawlike.
            // better solution - more frequent sampling.
            ZMP_x.push_back(wmg.X_tilde.x());
            ZMP_y.push_back(wmg.X_tilde.y());
            wmg.X_tilde.get_next_state (solver);
        }
        CoM_x.push_back(par.init_state.x());
        CoM_y.push_back(par.init_state.y());
    

        // feet position/orientation
        double left_foot_pos[3+1];
        double right_foot_pos[3+1];
        wmg.getFeetPositions (control_sampling_time_ms, left_foot_pos, right_foot_pos);
                
        left_foot_x.push_back(left_foot_pos[0]);
        left_foot_y.push_back(left_foot_pos[1]);
        left_foot_z.push_back(left_foot_pos[2]);

        right_foot_x.push_back(right_foot_pos[0]);
        right_foot_y.push_back(right_foot_pos[1]);
        right_foot_z.push_back(right_foot_pos[2]);
        
        next_preview_len_ms -= control_sampling_time_ms;
    }

    // feet positions    
    fprintf(file_op,"LFP = [\n");
    for (unsigned int i=0; i < left_foot_x.size(); i++)
    {
        fprintf(file_op, "%f %f %f;\n", left_foot_x[i], left_foot_y[i], left_foot_z[i]);
    }
    fprintf(file_op, "];\n\n plot3(LFP(:,1), LFP(:,2), LFP(:,3), 'r')\n");
    
    fprintf(file_op,"RFP = [\n");
    for (unsigned int i=0; i < right_foot_x.size(); i++)
    {
        fprintf(file_op, "%f %f %f;\n", right_foot_x[i], right_foot_y[i], right_foot_z[i]);
    }
    fprintf(file_op, "];\n\n plot3(RFP(:,1), RFP(:,2), RFP(:,3), 'r')\n");


    // ZMP
    fprintf(file_op,"ZMP = [\n");
    for (unsigned int i=0; i < ZMP_x.size(); i++)
    {
        fprintf(file_op, "%f %f;\n", ZMP_x[i], ZMP_y[i]);
    }
    fprintf(file_op, "];\n\n plot(ZMP(:,1), ZMP(:,2), 'k')\n");

    // reference ZMP points
    fprintf(file_op,"ZMPref = [\n");
    for (unsigned int i=0; i < ZMP_ref_x.size(); i++)
    {
        fprintf(file_op, "%f %f;\n", ZMP_ref_x[i], ZMP_ref_y[i]);
    }
    fprintf(file_op, "];\n\n plot(ZMPref(:,1), ZMPref(:,2), 'ko')\n");

    // CoM
    fprintf(file_op,"CoM = [\n");
    for (unsigned int i=0; i < CoM_x.size(); i++)
    {
        fprintf(file_op, "%f %f;\n", CoM_x[i], CoM_y[i]);
    }
    fprintf(file_op, "];\n\n plot(CoM(:,1), CoM(:,2), 'b')\n");


    fprintf(file_op,"hold off\n");
    fclose(file_op);
    test_end(argv[0]);

    return 0;
}
///@}
