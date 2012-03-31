/** 
 * @file
 * @author Alexander Sherikov
 * @date 19.07.2011 22:30:13 MSD
 */



/****************************************
 * INCLUDES 
 ****************************************/

#include "chol_solve_as.h"

#include <cmath> // sqrt
#include <cstring> // memset


/****************************************
 * FUNCTIONS 
 ****************************************/

//==============================================
// constructors / destructors

/**
 * @brief Constructor
 *
 * @param[in] N size of the preview window.
 */
chol_solve_as::chol_solve_as (const int N) : ecL(N)
{
    nu = new double[SMPC_NUM_VAR*N];
    XiHg = new double[SMPC_NUM_VAR*N];
    z = new double[SMPC_NUM_VAR*N];

    icL = new double*[N*2];
    icL_mem = new double[SMPC_NUM_VAR*N*N*2];
    for(int i = 0; i < N*2; i++)
    {
        icL[i] = &icL_mem[i * SMPC_NUM_VAR*N];
    }
}


chol_solve_as::~chol_solve_as()
{
    if (icL != NULL)
    {
        delete icL_mem;
        delete icL;
    }
    if (z != NULL)
        delete z;
    if (nu != NULL)
        delete nu;
    if (XiHg != NULL)
        delete XiHg;
}
//==============================================


/**
 * @brief Forms row vector 's_a' (@ref pCholUp).
 *
 * @param[in] ppar parameters
 * @param[in] c activated constraint
 * @param[in] ic_len the length of new row in L
 * @param[out] row 's_a' row
 */
void chol_solve_as::form_sa_row(
        const problem_parameters& ppar, 
        const constraint& c,
        const int ic_len, 
        double *row)
{
    double i2H = ppar.i2Q[0]; // a'*inv(H) = a'*inv(H)*a
    int first_num = c.ind; // first !=0 element


    // reset memory
    memset(row, 0, ic_len * sizeof(double));


    // a * iH * -I
    row[first_num] = -i2H * c.coef_x;
    row[first_num + 3] = -i2H * c.coef_y;

    if (first_num/SMPC_NUM_STATE_VAR != ppar.N-1)
    {
        // a * iH * A'
        row[first_num + 6] = i2H * c.coef_x;
        row[first_num + 9] = i2H * c.coef_y;
    }

    // initialize the last element in the row
    // coef_x^2 + coef_y^2 = 1, since they are row in a rotation matrix
    row[ic_len] = i2H;
}



/**
 * @brief Determines feasible descent direction.
 *
 * @param[in] ppar   parameters.
 * @param[in] i2Hg   inverted hessian * g.
 * @param[in] x    initial guess.
 * @param[out] dx   feasible descent direction, must be allocated.
 */
void chol_solve_as::solve(
        const problem_parameters& ppar, 
        const double *i2Hg,
        const double *x, 
        double *dx)
{
    double *s_nu = nu;
    int i;
    double i2Q[3] = {ppar.i2Q[0], ppar.i2Q[1], ppar.i2Q[2]};


    // generate L
    ecL.form (ppar);

    // -(x + inv(H) * g)
    //  x - initial feasible point
    for (i = 0; i < SMPC_NUM_VAR * ppar.N; i++)
    {
        XiHg[i] = -x[i];
    }
    for (i = 0; i < 2*ppar.N; i++)
    {
        XiHg[i*3] -= i2Hg[i];
    }

    // obtain s = E * x;
    E.form_Ex (ppar, XiHg, s_nu);

    // obtain nu
    ecL.solve_forward(ppar.N, s_nu);
    // make copy of z - it is constant
    for (i = 0; i < SMPC_NUM_STATE_VAR * ppar.N; i++)
    {
        z[i] = s_nu[i];
    }
    ecL.solve_backward(ppar.N, s_nu);

    // E' * nu
    E.form_ETx (ppar, s_nu, dx);

    
    // dx = -iH*(grad + E'*nu)
    //
    // dx = -(x + inv(H) * g + inv(H) * E' * nu)
    //        ~~~~~~~~~~~~~~            ~~~~~~~
    // dx   -(   -XiHg       + inv(H) *   dx   ) 
    for (i = 0; i < ppar.N*SMPC_NUM_STATE_VAR; i++)
    {
        // dx for state variables
        dx[i] = XiHg[i] - i2Q[i%3] * dx[i];
    }
    for (; i < ppar.N*SMPC_NUM_VAR; i++)
    {
        // dx for control variables
        dx[i] = XiHg[i] - ppar.i2P * dx[i];
    }
}


/**
 * @brief A wrapper around private functions, which update Cholesky factor and 
 *  resolve the system.
 *
 * @param[in] ppar   parameters.
 * @param[in] iHg   inverted hessian * g.
 * @param[in] constraints the set of constraints
 * @param[in] nW    number of added constrains.
 * @param[in] W     indicies of added constraints.
 * @param[in] x     initial guess.
 * @param[out] dx   feasible descent direction, must be allocated.
 */
void chol_solve_as::up_resolve(
        const problem_parameters& ppar, 
        const double *iHg,
        const std::vector <constraint>& constraints,
        const int nW, 
        const int *W, 
        const double *x, 
        double *dx)
{
    int ic_num = nW-1;
    int ic_ind = W[ic_num];
    constraint c = constraints[ic_ind];

    update (ppar, c, ic_num, ic_ind/2);
    update_z (ppar, iHg, c, ic_num, x);
    resolve (ppar, iHg, constraints, nW, W, x, dx);
}


/**
 * @brief Adds a row corresponding to some inequality constraint to L, see
 * '@ref pCholUpAlg'.
 *
 * @param[in] ppar parameters.
 * @param[in] c activated constraint
 * @param[in] ic_num index of added constraint in W
 * @param[in] state_num number of the state corresponding to the actived constraint
 */
void chol_solve_as::update (
        const problem_parameters& ppar, 
        const constraint& c,
        const int ic_num, 
        const int state_num)
{
    int i, j, k;

    double *new_row = icL[ic_num]; // current row in icL
    // trailing elements of new_row corresponding to active constraints
    double *new_row_end = &new_row[ppar.N*SMPC_NUM_STATE_VAR]; 

    int last_num = ic_num + ppar.N*SMPC_NUM_STATE_VAR; // the last !=0 element


    // form row 'a' in the current row of icL
    form_sa_row(ppar, c, last_num, new_row);


    // update elements starting from the first non-zero
    // element in the row to SMPC_NUM_STATE_VAR * N (size of ecL)
    // the calculation of the last elements is completed
    // in a separate loop
    // each number in row 'a' causes update of only 3 elements following
    // it, they can be 1,2,6; 1,5,6; 4,5,6
    for(i = state_num; i < ppar.N; i++)
    {                                  
        // variables corresponding to x and y are computed using the same matrices
        for (k = 0; k < 2; k++)
        {
            double* cur_pos = &new_row[i*SMPC_NUM_STATE_VAR + 3*k];

            // ----------------------------------------------------------------
            cur_pos[0] /= ecL.ecL_diag[i][0];
            cur_pos[1] -= cur_pos[0] * ecL.ecL_diag[i][1];
            cur_pos[1] /= ecL.ecL_diag[i][4];
            cur_pos[2] -= cur_pos[0] * ecL.ecL_diag[i][2] + cur_pos[1] * ecL.ecL_diag[i][5];
            cur_pos[2] /= ecL.ecL_diag[i][8];

            // make a copy for faster computations
            double tmp_copy_el[3] = {cur_pos[0], cur_pos[1], cur_pos[2]};

            // ----------------------------------------------------------------
            if (i < ppar.N - 1) // non-diagonal matrix of ecL cannot be
            {                   // used when the last state is processed

                // these elements can be updated here, since they are not 
                // used in computation of other elements on this iteration
                cur_pos[6] -= tmp_copy_el[0] * ecL.ecL_ndiag[i][0] 
                            + tmp_copy_el[1] * ecL.ecL_ndiag[i][3]
                            + tmp_copy_el[2] * ecL.ecL_ndiag[i][6];

                cur_pos[7] -= tmp_copy_el[1] * ecL.ecL_ndiag[i][4]
                            + tmp_copy_el[2] * ecL.ecL_ndiag[i][7];

                cur_pos[8] -= tmp_copy_el[2] * ecL.ecL_ndiag[i][8];
            }
            // update the last (diagonal) number in the row
            new_row[last_num] -= tmp_copy_el[0] * tmp_copy_el[0] 
                               + tmp_copy_el[1] * tmp_copy_el[1] 
                               + tmp_copy_el[2] * tmp_copy_el[2];

            // update elements after N*SMPC_NUM_STATE_VAR using the previously added rows
            // in icL
            for (j = 0; j < ic_num; j++)
            {
                new_row_end[j] -= tmp_copy_el[0] * icL[j][i*SMPC_NUM_STATE_VAR + 3*k]
                                + tmp_copy_el[1] * icL[j][i*SMPC_NUM_STATE_VAR + 3*k + 1]
                                + tmp_copy_el[2] * icL[j][i*SMPC_NUM_STATE_VAR + 3*k + 2];
            }
        }
    }

    // update elements in the end of icL
    for(i = SMPC_NUM_STATE_VAR * ppar.N, k = 0; i < last_num; i++, k++)
    {
        new_row[i] /= icL[k][i];
        double tmp_copy_el = new_row[i];

        // determine number in the row of L

        // update the last (diagonal) number in the row
        new_row[last_num] -= tmp_copy_el * tmp_copy_el;

        for (j = k+1; j < ic_num; j++)
        {
            new_row_end[j] -= tmp_copy_el * icL[j][i];
        }
    }

    // square root of the diagonal element
    new_row[last_num] = sqrt(new_row[last_num]);
}



/**
 * @brief Adjust vector '@ref pz "z"' after update.
 *
 * @param[in] ppar  parameters.
 * @param[in] i2Hg  inverted hessian * g.
 * @param[in] c     activated constraint
 * @param[in] ic_num number of added constraint in the active set
 * @param[in] x     initial guess.
 */
void chol_solve_as::update_z (
        const problem_parameters& ppar, 
        const double *i2Hg,
        const constraint& c,
        const int ic_num, 
        const double *x)
{
    // update lagrange multipliers
    int zind = ppar.N*SMPC_NUM_STATE_VAR + ic_num;
    // sn
    int first_num = c.ind; // first !=0 element
    int i2Hg_ind = 2 * first_num / SMPC_NUM_STATE_VAR;

    double zn = -(i2Hg[i2Hg_ind] + x[first_num]) * c.coef_x
                -(i2Hg[i2Hg_ind+1] + x[first_num+3]) * c.coef_y;

    int i;

    // zn
    for (i = 0; i < first_num; i++)
    {
        nu[i] = z[i];
    }
    for (; i < zind; i++)
    {
        zn -= z[i] * icL[ic_num][i];
        nu[i] = z[i];
    }
    nu[zind] = z[zind] = zn/icL[ic_num][zind];
    return;
}



/**
 * @brief Determines feasible descent direction with respect to added
 *  inequality constraints.
 *
 * @param[in] ppar   parameters.
 * @param[in] iHg   inverted hessian * g.
 * @param[in] constraints the set of constraints
 * @param[in] nW    number of added constrains.
 * @param[in] W     indicies of added constraints.
 * @param[in] x     initial guess.
 * @param[out] dx   feasible descent direction, must be allocated.
 */
void chol_solve_as::resolve (
        const problem_parameters& ppar, 
        const double *iHg,
        const std::vector <constraint>& constraints,
        const int nW, 
        const int *W, 
        const double *x, 
        double *dx)
{
    int i;

    // backward substitution for icL
    for (i = nW-1; i >= 0; i--)
    {
        int last_el_num = i + ppar.N*SMPC_NUM_STATE_VAR;
        nu[last_el_num] /= icL[i][last_el_num];

        for (int j = last_el_num - 1; j >= W[i]/2 * SMPC_NUM_STATE_VAR; j--)
        {
            nu[j] -= nu[last_el_num] * icL[i][j];
        }
    }
    // backward substitution for ecL
    ecL.solve_backward(ppar.N, nu);


    // E' * nu
    E.form_ETx (ppar, nu, dx);

    // dx = -iH*(grad + E'*nu  + A(W,:)'*lambda)
    //
    // dx = -(x + inv(H) * g + inv(H) * E' * nu)
    //            ~~~~~~~~~~            ~~~~~~~
    // dx   -(x +  iHg       + inv(H) *   dx   ) 
    double i2Q[3] = {ppar.i2Q[0], ppar.i2Q[1], ppar.i2Q[2]};

    for (i = 0; i < ppar.N*SMPC_NUM_STATE_VAR; i++)
    {
        // dx for state variables
        dx[i] = -x[i] - i2Q[i%3] * dx[i];
    }
    for (; i < ppar.N*SMPC_NUM_VAR; i++)
    {
        // dx for control variables
        dx[i] = -x[i] - ppar.i2P * dx[i];
    }
    for (i = 0; i < 2*ppar.N; i++)
    {
        // dx for state variables
        dx[i*3] -= iHg[i];
    }

    // -iH * A(W,:)' * lambda
    double *lambda = &nu[ppar.N*SMPC_NUM_STATE_VAR];
    for (i = 0; i < nW; i++)
    {
        int first_num = constraints[W[i]].ind;
        dx[first_num]   -= i2Q[0] * constraints[W[i]].coef_x * lambda[i];
        dx[first_num+3] -= i2Q[0] * constraints[W[i]].coef_y * lambda[i];
    }
}



/**
 * @brief A wrapper around private functions, which downdate Cholesky factor and 
 *  resolve the system.
 *
 * @param[in] ppar   parameters.
 * @param[in] iHg   inverted hessian * g.
 * @param[in] constraints the set of constraints
 * @param[in] nW    number of added constraints (without removed constraint).
 * @param[in] W     indicies of added constraints (without removed constraint).
 * @param[in] ind_exclude index of excluded constraint.
 * @param[in] x     initial guess.
 * @param[out] dx   feasible descent direction, must be allocated.
 *
 * @note Downdate of vector @ref pz 'z' is described on the page '@ref pRemoveICz'.
 */
void chol_solve_as::down_resolve(
        const problem_parameters& ppar, 
        const double *iHg,
        const std::vector <constraint>& constraints,
        const int nW, 
        const int *W, 
        const int ind_exclude, 
        const double *x, 
        double *dx)
{
    // for each element of z affected by removed constraint
    // find a base that stays the same
    double z_tmp = 0;
    for (int i = nW; i > ind_exclude; i--)
    {
        int zind = ppar.N*SMPC_NUM_STATE_VAR + i;
        double zn = z[zind] * icL[i][zind];
        z[zind] = z_tmp;

        for (int j = ppar.N*SMPC_NUM_STATE_VAR + ind_exclude; j < zind; j++)
        {
            zn += z[j] * icL[i][j];
        }
        z_tmp = zn;
    }
    z[ppar.N*SMPC_NUM_STATE_VAR + ind_exclude] = z_tmp;


    // downdate L
    downdate (ppar, nW, ind_exclude, x);


    // recompute elements of z
    for (int i = ind_exclude; i < nW; i++)
    {
        int zind = ppar.N*SMPC_NUM_STATE_VAR + i;
        double zn = z[zind];

        // zn
        // start from the first !=0 element
        for (int j = ppar.N*SMPC_NUM_STATE_VAR+ind_exclude; j < zind; j++)
        {
            zn -= z[j] * icL[i][j];
        }
        z[zind] = zn/icL[i][zind];
    }

    // copy z to nu
    for (int i = 0; i < ppar.N*SMPC_NUM_STATE_VAR + nW; i++)
    {
        nu[i] = z[i];
    }


    resolve (ppar, iHg, constraints, nW, W, x, dx);
}



/**
 * @return a pointer to the memory where current lambdas are stored.
 * @param[in] ppar parameters
 */
double * chol_solve_as::get_lambda(const problem_parameters& ppar)
{
    return(&nu[SMPC_NUM_STATE_VAR*ppar.N]);
}




/**
 * @brief Delete a line from icL, see page '@ref pCholDown'.
 *
 * @param[in] ppar   parameters.
 * @param[in] nW    number of added constrains.
 * @param[in] ind_exclude index of excluded constraint.
 * @param[in] x     initial guess.
 */
void chol_solve_as::downdate(
        const problem_parameters& ppar, 
        const int nW, 
        const int ind_exclude, 
        const double *x)
{
    // Shuffle memory pointers to avoid copying of the data.
    double * downdate_row = icL[ind_exclude];
    for (int i = ind_exclude + 1; i < nW + 1; i++)
    {
        icL[i-1] = icL[i];
    }
    icL[nW] = downdate_row;


    for (int i = ind_exclude; i < nW; i++)
    {
        int el_index = SMPC_NUM_STATE_VAR*ppar.N + i;
        double *cur_el = &icL[i][el_index];
        double x1 = cur_el[0];
        double x2 = cur_el[1];
        double cosT, sinT;


        // Givens rotation matrix
        if (abs(x2) >= abs(x1))
        {
            double t = x1/x2;
            sinT = 1/sqrt(1 + t*t);
            cosT = sinT*t;
        }
        else
        {
            double t = x2/x1;
            cosT = 1/sqrt(1 + t*t);
            sinT = cosT*t;
        }


        // update elements in the current line
        cur_el[0] = cosT*x1 + sinT*x2;
        cur_el[1] = 0;

        // change sign if needed (diagonal elements of Cholesky 
        // decomposition must be positive)
        double sign = copysign(1, cur_el[0]);
        cur_el[0] = fabs(cur_el[0]);

        // update the lines below the current one.
        for (int j = i + 1; j < nW; j++)
        {
            x1 = icL[j][el_index];
            x2 = icL[j][el_index + 1];

            icL[j][el_index] = sign * (cosT*x1 + sinT*x2);
            icL[j][el_index + 1] = -sinT*x1 + cosT*x2;
        }
    }
}
