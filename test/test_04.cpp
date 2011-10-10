/** 
 * @file
 * @author Alexander Sherikov
 * @brief Simulation with double support
 */


#include <string>
#include <cstring> //strcmp
#include "tests_common.h"

///@addtogroup gTEST
///@{

int main(int argc, char **argv)
{
    bool dump_to_stdout = false;
    ofstream fs_out;

    //-----------------------------------------------------------
    // initialize
    WMG wmg;
    init_03 (&wmg);

    string fs_out_filename("test_04_fs.m");
    wmg.FS2file(fs_out_filename); // output results for later use in Matlab/Octave
    //-----------------------------------------------------------


    if ((argc == 2) && (strcmp (argv[1], "stdout") == 0))
    {
        dump_to_stdout = true;
        cout.precision (numeric_limits<double>::digits10);
    }


    if (!dump_to_stdout)
        test_start(argv[0]);


    double X[6];
    fs_out.open(fs_out_filename.c_str(), fstream::app);
    fs_out.precision (numeric_limits<double>::digits10);
    fs_out << endl << endl;
    fs_out << "CoM_ZMP = [";


    smpc_solver solver(wmg.N);


    for(;;)
    {
        //------------------------------------------------------
        if (wmg.FormPreviewWindow() == WMG_HALT)
        {
            cout << "EXIT (halt = 1)" << endl;
            break;
        }
        //------------------------------------------------------


        //------------------------------------------------------
        solver.init(wmg.T, wmg.h, wmg.angle, wmg.zref_x, wmg.zref_y, wmg.lb, wmg.ub, wmg.X_tilde, wmg.X);
        solver.solve();
        solver.get_next_state_tilde (wmg.X_tilde);
        //------------------------------------------------------

        solver.get_next_state (X);
        fs_out << endl << X[0] << " " << X[3] << " " << wmg.X_tilde[0] << " " << wmg.X_tilde[3] << ";";
    
        if (dump_to_stdout)
        {
            for (int i = 0; i < wmg.N*NUM_VAR; i++)
            {
                cout << wmg.X[i] << endl;
            }
        }
    }

    fs_out << "];" << endl;
    fs_out << "plot (CoM_ZMP(:,1), CoM_ZMP(:,2), 'b');" << endl;
    fs_out << "plot (CoM_ZMP(:,3), CoM_ZMP(:,4), 'ks','MarkerSize',5);" << endl;
    fs_out.close();


    if (!dump_to_stdout)
        test_end(argv[0]);

    return 0;
}
///@}