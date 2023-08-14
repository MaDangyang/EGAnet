// This function was created with the assistance of GPT-4
// Much of the template for the code was generated by GPT-4
// Optimizations were developed after the template to streamline
// and make the code computationally fast.
//
// This implementation of polychoric correlations relies on numerical approximations
// for the inverse univariate normal CDF and bivariate normal CDF.
//
// Inverse Univariate Normal CDF: Beasley-Springer-Moro algorithm
//
// Bivariate Normal CDF: Drezner-Wesolowsky approximation
//
// Optimization method: Brent's method

// Headers to include
#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <float.h>
#include <stdlib.h>
#include <stddef.h>
#include <R.h>
#include <Rinternals.h>
#include "polychoric_matrix.h" // Constants are defined here

// Constants in `bsm_inverse_cdf`
const double CONST_A[6] = {-39.69683028665376, 220.9460984245205, -275.928510446969, 138.357751867269, -30.66479806614716, 2.506628277459239};
const double CONST_B[5] = {-54.47609879822406, 161.5858368580409, -155.6989798598866, 66.80131188771972, -13.28068155288572};
const double CONST_C[6] = {-0.007784894002430293, -0.3223964580411365, -2.400758277161838, -2.549732539343734, 4.374664141464968, 2.938163982698783};
const double CONST_D[4] = {0.007784695709041462, 0.3224671290700398, 2.445134137142996, 3.754408661907416};

// Constants in `drezner_bivariate_normal`
const double DOUBLE_X[5] = {0.04691008, 0.23076534, 0.5, 0.76923466, 0.95308992};
const double DOUBLE_W[5] = {0.018854042, 0.038088059, 0.0452707394, 0.038088059, 0.018854042};

// Using the Beasley-Springer-Moro algorithm to perform `qnorm` function
double bsm_inverse_cdf(double probability){

  // Check for zero and one probabilities
  if(probability == 0) return(-INFINITY);
  if(probability == 1) return(INFINITY);

  // Initialize variables once
  double q, r, x;
  
  // (Not) lower tail flag
  bool not_lower_tail = probability >= 0.02425;

  // Determine region
  if(not_lower_tail && probability <= 0.97575){ // Middle

    // Define q
    q = probability - 0.50;

    // Define r
    r = q * q;

    // Define x
    x = ((((((CONST_A[0] * r + CONST_A[1]) * r + CONST_A[2]) * r + CONST_A[3]) * r + CONST_A[4]) * r + CONST_A[5]) * q) / (((((CONST_B[0] * r + CONST_B[1]) * r + CONST_B[2]) * r + CONST_B[3]) * r + CONST_B[4]) * r + 1);

  } else { // Ends

    // Define q
    if(not_lower_tail){
      q = sqrt(-2 * log(1 - probability));
    } else {
      q = sqrt(-2 * log(probability));
    }

    // Define x
    x = (((((CONST_C[0] * q + CONST_C[1]) * q + CONST_C[2]) * q + CONST_C[3]) * q + CONST_C[4]) * q + CONST_C[5]) / ((((CONST_D[0] * q + CONST_D[1]) * q + CONST_D[2]) * q + CONST_D[3]) * q + 1);

    // Check if the sign needs to be reversed
    if(not_lower_tail){
      x = -x;
    }

  }

  // Return x
  return x;

}

// Obtain joint frequency table
int** joint_frequency_table(
    int* input_data, int rows, int i, int j, int* missing
) {

  // Initialize X, Y, and iterator
  int X, Y, k;

  // Allocate memory space for table
  int** joint_frequency = (int**) malloc(CUT * sizeof(int*));
  int* joint_frequency_data = (int*) calloc(CUT * CUT, sizeof(int));
  for (k = 0; k < CUT; k++) {
    joint_frequency[k] = &(joint_frequency_data[k * CUT]);
  }

  // Pre-compute matrix offset for i and j
  int matrix_offset_i = i * rows;
  int matrix_offset_j = j * rows;

  // Populate table
  for (k = 0; k < rows; k++) {

    // Obtain X and Y
    X = input_data[k + matrix_offset_i];
    Y = input_data[k + matrix_offset_j];

    // Check for missing data
    if(X != MISSING && Y != MISSING){
      joint_frequency[X][Y]++;
    }else{
      (*missing)++;
    }

  }

  // Return joint frequency table
  return joint_frequency;

}

// Update joint frequency table
/* Removes zero sum rows and columns and counts zero frequency cells */
double** update_joint_frequency (int** joint_frequency_max, int* cat_X, int* cat_Y, int* zero_count) {

  // Initialize iterators
  int i, j;
  
  // Count the non-zero rows and columns
  int non_zero_rows = 0;
  int non_zero_columns = 0;

  // Initialize zero rows and columns
  bool* zero_rows = (bool*) calloc(CUT, sizeof(bool));
  bool* zero_columns = (bool*) calloc(CUT, sizeof(bool));

  // Loop over rows
  for(i = 0; i < CUT; i++) {

    // Initialize row and column sums
    int row_sum = 0;
    int column_sum = 0;

    // Loop over columns
    for(j = 0; j < CUT; j++) {

      // Increase sums
      row_sum += joint_frequency_max[i][j];
      column_sum += joint_frequency_max[j][i];

    }

    // Check for zero row sums
    if(row_sum == 0){
      zero_rows[i] = true;
    }else{
      non_zero_rows++;
    }
    
    // Check for zero column sums
    if(column_sum == 0){
      zero_columns[i] = true;
    }else{
      non_zero_columns++;
    }

  }

  // Create new joint frequency table
  double** joint_frequency_trim = (double**) malloc(non_zero_rows * sizeof(double*));
  double* joint_frequency_data = (double*) calloc(non_zero_rows * non_zero_columns, sizeof(double));
  for (i = 0; i < non_zero_rows; i++) {
    joint_frequency_trim[i] = &(joint_frequency_data[i * non_zero_columns]);
  }

  // Initialize row count
  int row_count = 0;

  // Populate trimmed table
  for(i = 0; i < CUT; i++){

    // Check for zero row
    if(zero_rows[i]) {
      continue;
    }

    // Initialize column count
    int column_count = 0;

    for(j = 0; j < CUT; j++){

      // Check for zero column
      if(zero_columns[j]) {
        continue;
      }

      // Populate table
      joint_frequency_trim[row_count][column_count] = joint_frequency_max[i][j];

      // Increase zero count
      if(joint_frequency_max[i][j] == 0){
        (*zero_count)++;
      }

      // Increase column
      column_count++;

    }

    // Increase row
    row_count++;

  }

  // Assign non-zero values to categories
  *cat_X = non_zero_rows;
  *cat_Y = non_zero_columns;

  // Free memory
  free(zero_rows);
  free(zero_columns);

  // Return frequency table
  return joint_frequency_trim;

}

// Define structure for return values
struct ThresholdsResult {
  double** joint_frequency;
  double* threshold_X;
  double* threshold_Y;
  double* probability_X;
  double* probability_Y;
  int cat_X;
  int cat_Y;
};

// Compute thresholds
struct ThresholdsResult thresholds(int* input_data, int rows, int i, int j, int empty_method, double empty_value) {

  // Initialize iterators
  int k, l;

  // Initialize missing
  int missing = 0;

  // Obtain joint frequency table
  int** joint_frequency_max = joint_frequency_table(input_data, rows, i, j, &missing);

  // Initialize categories
  int cat_X = 0;
  int cat_Y = 0;

  // Initialize zero count
  int zero_count = 0;

  // Update joint frequency table (remove zero rows)
  double** joint_frequency = update_joint_frequency(joint_frequency_max, &cat_X, &cat_Y, &zero_count);

  // Free memory
  free(joint_frequency_max[0]);
  free(joint_frequency_max);

  // Initialize added value
  double added_value = 0.0;
  double added_sum = 0.0;

  // Check for zero cells
  if(zero_count > 0){

    // If there are zero cells, check method
    if(empty_method != 0){
      
      // Flag for empty cell methods
      bool zero = empty_method == 1;
      // bool all = empty_method == 2;

      // Check for one over method
      if(empty_value > 1.0){
        added_value = 1 / (double) (cat_X * cat_Y); // needs conversion to double before division
      }else{
        added_value = empty_value;
      }

      // Update joint frequency table
      for (k = 0; k < cat_X; k++) {
        for(l = 0; l < cat_Y; l++){

          // Check for method
          if(zero && joint_frequency[k][l] == 0){
            joint_frequency[k][l] += added_value;
            added_sum += added_value;
          } else { // if(all){
            joint_frequency[k][l] += added_value;
            added_sum += added_value;
          }

        }
      }

    }

  }

  // Initialize memory space for frequencies
  double* frequency_X = (double*) calloc(cat_X, sizeof(double));
  double* frequency_Y = (double*) calloc(cat_Y, sizeof(double));

  // Obtain frequencies
  for(k = 0; k < cat_X; k++) {
    for(l = 0; l < cat_Y; l++) {
      frequency_X[k] += joint_frequency[k][l];
      frequency_Y[l] += joint_frequency[k][l];
    }
  }

  // Initialize memory space for probabilities and thresholds
  double* probability_X = (double*) malloc(cat_X * sizeof(double));
  double* probability_Y = (double*) malloc(cat_Y * sizeof(double));
  double* threshold_X = (double*) malloc(cat_X * sizeof(double));
  double* threshold_Y = (double*) malloc(cat_Y * sizeof(double));

  // Compute cases
  double cases = rows - missing + added_sum;

  // Compute probabilities
  for(k = 0; k < cat_X; k++) {
      probability_X[k] = frequency_X[k] / cases;
  }
  for(k = 0; k < cat_Y; k++) {
      probability_Y[k] = frequency_Y[k] / cases;
  }

  // Free the memory for frequency_X and frequency_Y
  free(frequency_X);
  free(frequency_Y);

  // Compute cumulative sums
  for (k = 1; k < cat_X; k++) {
      probability_X[k] += probability_X[k - 1];
  }
  for (k = 1; k < cat_Y; k++) {
      probability_Y[k] += probability_Y[k - 1];
  }

  // Obtain thresholds
  for (k = 0; k < cat_X; k++) {
      threshold_X[k] = bsm_inverse_cdf(probability_X[k]);
  }
  for (k = 0; k < cat_Y; k++) {
      threshold_Y[k] = bsm_inverse_cdf(probability_Y[k]);
  }

  // Create structure for return values
  struct ThresholdsResult result;
  result.joint_frequency = joint_frequency;
  result.threshold_X = threshold_X;
  result.threshold_Y = threshold_Y;
  result.probability_X = probability_X;
  result.probability_Y = probability_Y;
  result.cat_X = cat_X;
  result.cat_Y = cat_Y;

  // Return
  return result;
}

// Error function
double error_function(double x) {

  // Initialize values
  double t_value, y;

  // Determine sign
  int sign_x = (x >= 0) ? 1 : -1;

  // Obtain absolute value
  x = fabs(x);

  // Set t-value
  t_value = 1 / (1 + P * x);

  // Set y
  y = 1 - (((((A5 * t_value + A4) * t_value) + A3) * t_value + A2) * t_value + A1) * t_value * exp(-x * x);

  // Add sign
  return sign_x * y;
}

// Univariate normal CDF
static inline double univariate_normal(double x) {

  // This function is streamlined for use in this function
  // With mean = 0 and sd = 1, then the z-score of x is x

  // Return CDF (sqrt(2) = 1.41421356)
  return 0.5 * (1 + error_function(x / 1.41421356));

}

// Bivariate normal CDF
// Implements Drezner-Wesolowsky's approximation (Drezner & Wesolowsky, 1990)
// Translated from C++ to C: https://github.com/cran/pbv/blob/master/src/pbv_rcpp_bvnorm.cpp
double drezner_bivariate_normal(double h1, double h2, double rho, double p1, double p2) {

  // Check for infinities
  if(h1 == -INFINITY || h2 == -INFINITY) return(0.0);
  if(h1 == INFINITY) return(p2);
  if(h2 == INFINITY) return(p1);

  // Initialize iterator
  // int i; // not used with loops unrolled

  // Initialize probability and h3
  double bv = 0.0;
  double h3;

  // Obtain h12 and absolute correlation
  double h12 = (h1 * h1 + h2 * h2) / 2;
  double rho_abs = fabs(rho);

  // Check for correlation lower than maximum
  if(rho_abs <= COR_MAX) {
  
    // Initialize r1 and rr2
    double r1, rr2;
    
    // Compute h3
    h3 = h1 * h2;
    
    // Standard loop
    
    // // Compute probability
    // for (i = 0; i < INT_NX; i++) {
    //   r1 = rho * DOUBLE_X[i];
    //   rr2 = 1 - r1 * r1;
    //   bv += DOUBLE_W[i] * exp((r1 * h3 - h12) / rr2) / sqrt(rr2);
    // }
    
    // Unrolled loop
    
    // Compute probability
    r1 = rho * DOUBLE_X[0];
    rr2 = 1 - r1 * r1;
    bv += DOUBLE_W[0] * exp((r1 * h3 - h12) / rr2) / sqrt(rr2);
    
    r1 = rho * DOUBLE_X[1];
    rr2 = 1 - r1 * r1;
    bv += DOUBLE_W[1] * exp((r1 * h3 - h12) / rr2) / sqrt(rr2);
    
    r1 = rho * DOUBLE_X[2];
    rr2 = 1 - r1 * r1;
    bv += DOUBLE_W[2] * exp((r1 * h3 - h12) / rr2) / sqrt(rr2);
    
    r1 = rho * DOUBLE_X[3];
    rr2 = 1 - r1 * r1;
    bv += DOUBLE_W[3] * exp((r1 * h3 - h12) / rr2) / sqrt(rr2);
    
    r1 = rho * DOUBLE_X[4];
    rr2 = 1 - r1 * r1;
    bv += DOUBLE_W[4] * exp((r1 * h3 - h12) / rr2) / sqrt(rr2);
    
    // Finalize probability
    bv = p1 * p2 + rho * bv;

  } else { // Greater than maximum correlation (0.70)

    // Initialize r2 and r3
    double r2 = 1 - rho * rho;
    double r3 = sqrt(r2);

    // Reverse sign for negative correlation
    if(rho < 0.0) {
      h2 = -h2;
      p2 = 1 - p2;
    }

    // Compute h3
    h3 = h1 * h2;

    // Compute h7
    double h7 = exp(-h3 / 2.0);

    // Check for correlation less than 1
    if(rho_abs < 1.0) {

      // Set up variables
      double h6 = fabs(h1 - h2);
      double h5 = h6 * h6 / 2.0;
      h6 = h6 / r3;
      double aa = 0.5 - h3 / 8.0;
      double ab = 3.0 - 2.0 * aa * h5;

      // Compute initial probability
      bv = BV_FAC1 * h6 * ab * (1.0 - univariate_normal(h6)) - exp(-h5 / r2) * (ab + aa * r2) * BV_FAC2;

      double r1, rr;
      
      // Standard loop

      // // Compute probability
      // for (i = 0; i < INT_NX; i++) {
      //   r1 = r3 * DOUBLE_X[i];
      //   rr = r1 * r1;
      //   r2 = sqrt(1.0 - rr);
      //   bv += -DOUBLE_W[i] * exp(-h5 / rr) * (exp(-h3 / (1.0 + r2)) / r2 / h7 - 1.0 - aa * rr);
      // }
      
      // Unrolled loop
      
      // Compute probability
      r1 = r3 * DOUBLE_X[0];
      rr = r1 * r1;
      r2 = sqrt(1.0 - rr);
      bv += -DOUBLE_W[0] * exp(-h5 / rr) * (exp(-h3 / (1.0 + r2)) / r2 / h7 - 1.0 - aa * rr);
      
      r1 = r3 * DOUBLE_X[1];
      rr = r1 * r1;
      r2 = sqrt(1.0 - rr);
      bv += -DOUBLE_W[1] * exp(-h5 / rr) * (exp(-h3 / (1.0 + r2)) / r2 / h7 - 1.0 - aa * rr);
      
      r1 = r3 * DOUBLE_X[2];
      rr = r1 * r1;
      r2 = sqrt(1.0 - rr);
      bv += -DOUBLE_W[2] * exp(-h5 / rr) * (exp(-h3 / (1.0 + r2)) / r2 / h7 - 1.0 - aa * rr);
      
      r1 = r3 * DOUBLE_X[3];
      rr = r1 * r1;
      r2 = sqrt(1.0 - rr);
      bv += -DOUBLE_W[3] * exp(-h5 / rr) * (exp(-h3 / (1.0 + r2)) / r2 / h7 - 1.0 - aa * rr);
      
      r1 = r3 * DOUBLE_X[4];
      rr = r1 * r1;
      r2 = sqrt(1.0 - rr);
      bv += -DOUBLE_W[4] * exp(-h5 / rr) * (exp(-h3 / (1.0 + r2)) / r2 / h7 - 1.0 - aa * rr);

    }

    // Obtain minimum of probabilities
    bv = bv * r3 * h7 + fmin(p1, p2);

    // Adjust for negative correlation
    if (rho < 0) {
      bv = p1 - bv;
    }

  }

  // Return probability
  return bv;

}

// Estimate log-likelihood
double polychoric_log_likelihood(
    double rho, double** joint_frequency,
    double* threshold_X, double* threshold_Y,
    double* probability_X, double* probability_Y,
    int cat_X, int cat_Y
) {

  // Set upper and lowers (thresholds and probabilities)
  double lower_ti, upper_ti, lower_pi, upper_pi;
  double lower_tj, upper_tj, lower_pj, upper_pj;

  // Initialize variables
  double log_likelihood = 0.0;
  double probability;

  // Initialize iterators
  int i, j;

  // Compute log-likelihood
  for (i = 0; i < cat_X; ++i) {

    // Set up thresholds and probabilities
    // X
    // Initialize X values
    lower_ti = threshold_X[i - 1]; lower_pi = probability_X[i - 1];
    upper_ti = threshold_X[i]; upper_pi = probability_X[i];

    // Update on end cases
    // Lowest category
    if(i == 0){
      lower_ti = -INFINITY; lower_pi = 0;
    }
    // Highest category
    if(i == cat_X - 1){
      upper_ti = INFINITY; upper_pi = 1;
    }

    for (j = 0; j < cat_Y; ++j) {

      // Set up thresholds and probabilities
      // Y
      // Initialize Y values
      lower_tj = threshold_Y[j - 1]; lower_pj = probability_Y[j - 1];
      upper_tj = threshold_Y[j]; upper_pj = probability_Y[j];

      // Update on end cases
      // Lowest category
      if(j == 0){
        lower_tj = -INFINITY; lower_pj = 0;
      }
      // Highest category
      if(j == cat_Y - 1){
        upper_tj = INFINITY; upper_pj = 1;
      }

      // Compute bivariate normal CDF
      probability = drezner_bivariate_normal(upper_ti, upper_tj, rho, upper_pi, upper_pj) -
                    drezner_bivariate_normal(lower_ti, upper_tj, rho, lower_pi, upper_pj) -
                    drezner_bivariate_normal(upper_ti, lower_tj, rho, upper_pi, lower_pj) +
                    drezner_bivariate_normal(lower_ti, lower_tj, rho, lower_pi, lower_pj);

      // Handle probabilities equal to or less than zero
      // Solves issue when negative probability super close to zero
      if (probability <= 0 || isnan(probability)) {
        probability = DBL_MIN;
      }

      // Update log-likelihood
      log_likelihood += joint_frequency[i][j] * log(probability);

    }

  }

  // Return negative log-likelihood
  return -log_likelihood;

}

// Brent's method for optimization
double optimize(double (*f)(double, double**, double*, double*, double*, double*, int, int),
                double** joint_frequency, double* threshold_X, double* threshold_Y,
                double* probability_X, double* probability_Y,
                int cat_X, int cat_Y
) {

  // Initialize variables for the optimization algorithm
  double a = LOWER;
  double b = UPPER;
  double c = a + (b - a) / 2.0;

  double x = c;
  double w = c;
  double v = c;

  double fx = f(x, joint_frequency, threshold_X, threshold_Y, probability_X, probability_Y, cat_X, cat_Y);
  double fw = fx;
  double fv = fx;

  double d = 0.0;
  double e = 0.0;

  // Iterate using Brent's method until the maximum number of iterations
  // is reached, or until the solution is found within the specified tolerance
  for (int iter = 0; iter < MAX_ITER; ++iter) {

    // Calculate the midpoint of the current interval and the tolerance
    double mid = (a + b) / 2.0;
    double tol1 = TOL * fabs(x) + ZEPS;
    double tol2 = 2.0 * tol1;

    // Check if the solution is within the tolerance
    if (fabs(x - mid) <= (tol2 - (b - a) / 2.0)) {
      break;
    }

    // Initialize variables for the current iteration
    double u;
    double fu;
    bool use_parabola = false;

    // Check if the current iteration can use the parabolic fit
    if (x != w && x != v && w != v) {
      double r = (x - w) * (fx - fv);
      double q = (x - v) * (fx - fw);
      double p = (x - v) * q - (x - w) * r;

      q = 2.0 * (q - r);

      if (q > 0.0) {
        p = -p;
      } else {
        q = -q;
      }

      double etemp = e;
      e = d;

      // If the parabolic fit is not appropriate, set the use_parabola flag to false
      if (fabs(p) >= fabs(0.5 * q * etemp) || p <= q * (a - x) || p >= q * (b - x)) {
        use_parabola = false;
      } else {
        // Otherwise, set the use_parabola flag to true and calculate the parabolic minimum
        use_parabola = true;
        d = p / q;
        u = x + d;
        if (u - a < tol2 || b - u < tol2) {
          d = (mid - x >= 0.0) ? tol1 : -tol1;
        }
      }
    }

    // If the parabolic fit is not used, set the variables for the golden section step
    if (!use_parabola) {
      e = (x >= mid) ? a - x : b - x;
      d = 0.3819660 * e;
    }

    // Calculate the function value at the trial point
    u = (fabs(d) >= tol1) ? x + d : x + ((d >= 0.0) ? tol1 : -tol1);
    fu = f(u, joint_frequency, threshold_X, threshold_Y, probability_X, probability_Y, cat_X, cat_Y);

    // Update the intervals based on the function values at the trial point
    if (fu <= fx) {
      if (u >= x) {
        a = x;
      } else {
        b = x;
      }
      v = w;
      w = x;
      x = u;
      fv = fw;
      fw = fx;
      fx = fu;
    } else {
      if (u < x) {
        a = u;
      } else {
        b = u;
      }
      if (fu <= fw || w == x) {
        v = w;
        w = u;
        fv = fw;
        fw = fu;
      } else if (fu <= fv || v == x || v == w) {
        v = u;
        fv = fu;
      }
    }

  }

  // Return the optimal solution
  return x;
}

// Compute polychoric correlation
double polychoric(int* input_data, int rows, int i, int j, int empty_method, double empty_value) {

  // Obtain joint frequency table, probability_X, and probability_Y from thresholds function
  struct ThresholdsResult thresholds_result = thresholds(input_data, rows, i, j, empty_method, empty_value);

  // Perform optimization
  double rho_optimum = optimize(
    polychoric_log_likelihood, thresholds_result.joint_frequency,
    thresholds_result.threshold_X, thresholds_result.threshold_Y,
    thresholds_result.probability_X, thresholds_result.probability_Y,
    thresholds_result.cat_X, thresholds_result.cat_Y
  );

  // Free memory
  free(thresholds_result.joint_frequency[0]);
  free(thresholds_result.joint_frequency);
  free(thresholds_result.threshold_X);
  free(thresholds_result.threshold_Y);
  free(thresholds_result.probability_X);
  free(thresholds_result.probability_Y);

  // Return
  return rho_optimum;

}

// The updated polychoric_correlation_matrix function
void polychoric_correlation_matrix(
    int* input_data, int rows, int cols,
    int empty_method, double empty_value, double* polychoric_matrix
) {
  
  // Initialize iterators
  int i, j;
  double correlation;
  int matrix_offset[cols];
  matrix_offset[0] = 0;
  
  // Initialize offset
  for(i = 1; i < cols; i++){
    matrix_offset[i] = matrix_offset[i - 1] + cols;
  }
  
  // Perform polychoric correlations over the input_matrix
  for (i = 0; i < cols; i++) {
    
    // Fill diagonal
    polychoric_matrix[matrix_offset[i] + i] = 1;
    
    // Loop over other variables
    for (j = i + 1; j < cols; j++) {
      
      // Compute correlation
      correlation = polychoric(
        input_data, rows, i, j, empty_method, empty_value
      );
      
      // Add to matrix
      polychoric_matrix[matrix_offset[i] + j] = correlation;
      
      // Fill opposite of triangle
      polychoric_matrix[matrix_offset[j] + i] = correlation;
      
    }
  }
  
}

// Interface with R
SEXP r_polychoric_correlation_matrix(
    SEXP r_input_matrix, SEXP r_empty_method, 
    SEXP r_empty_value, SEXP r_rows, SEXP r_cols
) {
  
  // Initialize columns
  int cols = INTEGER(r_cols)[0];
  
  // Initialize R result
  SEXP r_result = PROTECT(allocVector(REALSXP, cols * cols));
  double* c_result = REAL(r_result);
  
  // Call the C function
  polychoric_correlation_matrix(
    INTEGER(r_input_matrix), INTEGER(r_rows)[0], cols,
    INTEGER(r_empty_method)[0], REAL(r_empty_value)[0],
    c_result // Pass the pointer directly to the C function
  );
  
  // Free R result
  UNPROTECT(1);
  
  // Return R result
  return r_result;
  
}
