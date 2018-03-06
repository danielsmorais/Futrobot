#include <iostream>
#include <complex>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "matrix.h"
#include "mymatrix.h"

// A fun��o default de tratamento de erros

static void matrix_msg(MATRIX_STATUS err, const char *msg)
{
  std::cout << "matrix: " << msg << '\n';
  // Eliminar o exit se n�o quiser que pare nos erros...
  exit(err);
}

// A fun��o que � chamada pelos m�todos da classe matrix quando h� um erro
// Esta fun��o chama a fun��o contida na vari�vel "matrix_error"

MATRIX_STATUS matrix_error_int(MATRIX_STATUS err)
{
  const char *msgerr;

  switch(err) {
  case MATRIX_OK:
    return err;
  case MATRIX_MEMORY_ALLOCATION:
    msgerr = "Memory allocation error";
    break;
  case MATRIX_BAD_PARAMETER:
    msgerr = "Bad parameter";
    break;
  case MATRIX_WRONG_DIMENSION:
    msgerr = "Wrong dimension(s)";
    break;
  case MATRIX_OUT_OF_RANGE:
    msgerr = "Index out of range";
    break;
  case MATRIX_NO_CONVERGENCE:
    msgerr = "No convergence after iterations";
    break;
  case MATRIX_DIVISION_ZERO:
    msgerr = "Division by zero";
    break;
  case MATRIX_NOT_SQUARE:
    msgerr = "Not square matrix";
    break;
  case MATRIX_SINGULARITY:
    msgerr = "Singular matrix";
    break;
  default:
    msgerr = "Undefined error";
    break;
  }
  matrix_error(err,msgerr);
  return(err);
}

// Vari�vel que cont�m a fun��o tratadora de erros
// Se o usu�rio quiser mudar a maneira de tratar os erros, deve definir
// uma nova fun��o que receba os mesmos par�metros ("err" e "msg") e
// em seguida atribuir esta fun��o � vari�vel "matrix_error"

void (*matrix_error)(MATRIX_STATUS err, const char *msg) =
matrix_msg;
