#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#define SV_IMPLEMENTATION
#include "../sv.h"

typedef enum {
	EXPR_KIND_NUMBER = 0,
	EXPR_KIND_CELL,
	EXPR_KIND_PLUS,
} Expr_Kind;

typedef struct Expr Expr;

typedef struct {
	Expr *lhs;
	Expr *rhs;
} Expr_Plus;

typedef struct {
	size_t col;
	size_t row;
} Expr_Cell;

typedef union {
	double number;
	Expr_Cell cell;
	Expr_Plus plus;
} Expr_As;

struct Expr {
	Expr_Kind kind;
	Expr_As as;
};

typedef enum {
	CELL_KIND_TEXT = 0,
	CELL_KIND_NUMBER,
	CELL_KIND_EXPR,
} Cell_Kind;

const char *cell_kind_as_cstr(Cell_Kind kind){

	switch(kind){
	case CELL_KIND_TEXT:
		return "TEXT";
	case CELL_KIND_NUMBER:
		return "NUMBER";
	case CELL_KIND_EXPR:
		return "EXPR";
	default:
		assert(0 && "unreachable");
		exit(1);
	}
}

typedef enum {
	UNEVALUATED = 0,
	INPROGRESS,
	EVALUATED,
}Eval_Status;

typedef struct {
	Expr *ast;
	Eval_Status status;
	double value;
} Cell_Expr;

typedef union {
	String_View text;
	double number;
	Cell_Expr expr;
} Cell_As;

typedef struct {
	Cell_Kind kind;
	Cell_As as;
} Cell;

typedef struct {
	Cell *cells;
	size_t rows;
	size_t cols;
} Table;

bool is_name(char c){
	return isalnum(c) || c == '_';
}

String_View next_token(String_View *source){
	*source = sv_trim(*source);

	if(source->count == 0){
		return SV_NULL;
	}
	
	if(*source->data == '+'){
		return sv_chop_left(source, 1);
	}

	if(is_name(*source->data)){
		return sv_chop_left_while(source, is_name);
	}

	fprintf(stderr, "ERROR: unknown token starts with '%c'", *source->data);
	exit(1);
}

bool sv_strtod(String_View source ,double *out){
	static char temp_buffer[1024 * 4];
	assert(source.count < sizeof(temp_buffer));
	snprintf(temp_buffer,sizeof(temp_buffer), SV_Fmt, SV_Arg(source));
	char *endptr = NULL;
	double result = strtod(temp_buffer, &endptr);
	if(out) *out =result;
	return (endptr != temp_buffer && *endptr == '\0');
}

bool sv_strtol(String_View source ,long int *out){
	static char temp_buffer[1024 * 4];
	assert(source.count < sizeof(temp_buffer));
	snprintf(temp_buffer,sizeof(temp_buffer), SV_Fmt, SV_Arg(source));
	char *endptr = NULL;
	long int result = strtol(temp_buffer, &endptr, 10);
	if(out) *out =result;
	return (endptr != temp_buffer && *endptr == '\0');
}

Expr *parse_primary_expr(String_View *source){
	String_View token = next_token(source);
	if(token.data == 0){
		fprintf(stderr, "ERROR: expected primary expression token, but got end of input \n");
		exit(1);
	}

				Expr *expr = malloc(sizeof(Expr));
				memset(expr, 0, sizeof(Expr));
				
	 			if(sv_strtod(token, &expr->as.number)){
					expr->kind = EXPR_KIND_NUMBER;
					return expr;
				} else {
					expr->kind = EXPR_KIND_CELL;
					if(!isupper(*token.data)){
						fprintf(stderr, "ERROR: cell reference must start with capital letter");
						exit(1);
					}
					expr->as.cell.col = *token.data - 'A';
					sv_chop_left(&token, 1);
					long int row;
					if(!sv_strtol(token, &row)){
						fprintf(stderr, "ERROR: cell reference must have an integer as the row number\n");
						exit(1);
					}

					expr->as.cell.row = (size_t) row;
					
				}
				return expr;
				// 2:22:03
}

Expr *parse_plus_expr(String_View *source){
	Expr *lhs = parse_primary_expr(source);

	String_View token = next_token(source);
	if(token.data != NULL && sv_eq(token, SV("+"))){
		Expr *rhs = parse_plus_expr(source);
		Expr *expr = malloc(sizeof(Expr));
		memset(expr, 0, sizeof(Expr));
		expr->kind = EXPR_KIND_PLUS;
		expr->as.plus.lhs = lhs;
		expr->as.plus.rhs = rhs;
		return expr;
	}
	return lhs;
}

void dump_expr(FILE *stream , Expr *expr, int level){

	fprintf(stream, "%*s", level*2, "");
	switch(expr->kind){
	case EXPR_KIND_NUMBER:
		fprintf(stream ,"NUMBER: %lf\n", expr->as.number);
		break;
	case EXPR_KIND_CELL:
		fprintf(stream, "CELL (%lu, %lu)\n",expr->as.cell.row, expr->as.cell.col);
		break;
	case EXPR_KIND_PLUS:
		fprintf(stream, "PLUS:\n");
		dump_expr(stream , expr->as.plus.lhs, level+1);
		dump_expr(stream , expr->as.plus.rhs, level+1);
		break;
	}	
}

Expr *parse_expr(String_View *source){
	return parse_plus_expr(source);
}

Table table_alloc(size_t rows, size_t cols){
	Table table = {0};
	table.rows = rows;
	table.cols = cols;
	
	// Allocate memory to store table
	table.cells = malloc(sizeof(Cell) * rows * cols);
	memset(table.cells, 0, sizeof(Cell) * rows * cols);
	if (table.cells == NULL){
		fprintf(stderr,"ERROR: could not allocate memory for the table \n");
		exit(1);
	}

	// Fill the table with zeros;
	memset(table.cells, 0 , sizeof(Cell) * rows * cols);
	return table;
}

Cell *table_cell_at(Table *table, size_t row, size_t col){
	assert(row < table->rows);
	assert(col < table->cols);
	return &table->cells[row * table->cols + col];
}

void usage(FILE *stream)
{
	fprintf(stream, "Usage: ./minicel <input.csv>\n");
}

char *slurp_file(const char *file_path, size_t *size)
{
	char *buffer = NULL;
	FILE *f = fopen(file_path, "rb");
	if(f == NULL){
		goto error;
	}

	if(fseek(f, 0, SEEK_END) < 0){
		goto error;
	}

	long m = ftell(f);
	if(m < 0){
		goto error;
	}

	buffer = malloc(sizeof(char) * m);
	if(buffer == NULL){
		goto error;
	}
	
	if(fseek(f, 0, SEEK_SET) < 0){
		goto error;
	}

	size_t n = fread(buffer, 1, m, f);
	assert(n == (size_t) m);
	if(ferror(f)){
		goto error;
	}

	if(size){
		*size = n;
	}
	
	fclose(f);
	
	return buffer;
 error:
	if(f){
		fclose(f);
	}
	if(buffer){
		free(buffer);
	}
	return NULL;
}


void parse_table_from_content(Table *table, String_View content){
	for(size_t row = 0 ; row < content.count; ++row){
		String_View line = sv_chop_by_delim(&content, '\n');
		for(size_t col = 0; col < line.count; ++col){
			String_View cell_value = sv_trim(sv_chop_by_delim(&line, '|'));
			Cell *cell = table_cell_at(table, row,col);
			
			if(sv_starts_with(cell_value,SV("="))) {
				sv_chop_left(&cell_value, 1);
				cell->kind = CELL_KIND_EXPR;
				cell->as.expr.ast = parse_expr(&cell_value);
			}  else {
				/* static char temp_buffer[1024 * 4]; */
				/* assert(cell_value.count < sizeof(temp_buffer)); */
				/* snprintf(temp_buffer,sizeof(temp_buffer), SV_Fmt, SV_Arg(cell_value)); */
				/* char *endptr; */
				//cell->as.number = strtod(temp_buffer, &endptr);

				//if(endptr != temp_buffer && *endptr == '\0'){
				if(sv_strtod(cell_value,&cell->as.number )){
					cell->kind = CELL_KIND_NUMBER;
				} else {
					cell->kind = CELL_KIND_TEXT;
					cell->as.text = cell_value;
				}
			}
		}
	}
}


void estimate_table_size(String_View content, size_t *out_rows, size_t *out_cols){

	size_t rows = 0;
	size_t cols = 0;

	for(; rows < content.count; ++rows){
		String_View line = sv_chop_by_delim(&content,'\n');
		size_t col = 0;
		for(; col < line.count; ++col){
			sv_chop_by_delim(&line, '|');
		}

		if(cols < col){
			cols = col;
		}
	}
	
	if(out_rows){
		*out_rows = rows;
	}

	if(out_cols){
		*out_cols = cols;
	}
}

/* int main(){ */
/* 	String_View source = SV_STATIC("A1+B1 + 80 + C1 + D1"); */
/*  	Expr *expr = parse_expr(&source); */
/* 	dump_expr(stdout, expr, 0); */
/* 	return 0; */
/* }  */
void table_eval_cell(Table *table, Cell *cell);

double table_eval_expr(Table *table, Expr *expr){
	switch(expr->kind){
	case EXPR_KIND_NUMBER:
		return expr->as.number;
	case EXPR_KIND_CELL:{
		Cell *cell = table_cell_at(table, expr->as.cell.row, expr->as.cell.col);
		switch(cell->kind){
		case CELL_KIND_NUMBER:
			return cell->as.number;
		case CELL_KIND_TEXT:
			fprintf(stderr, "ERROR: CELL(%zu : %zu)", expr->as.cell.row, expr->as.cell.col);
			exit(1);
			break;
		case CELL_KIND_EXPR:
			table_eval_cell(table, cell);
			return cell->as.expr.value;
		}
	}
		break;
	case EXPR_KIND_PLUS: {
		double lhs = table_eval_expr(table, expr->as.plus.lhs);
		double rhs = table_eval_expr(table, expr->as.plus.rhs);
	  return lhs + rhs;
	}	break;
	}
	return 0;
}
	
void table_eval_cell(Table *table, Cell *cell){
	
	if(cell->kind == CELL_KIND_EXPR){

		if(cell->as.expr.status == INPROGRESS){
			fprintf(stderr,"ERROR: Circular dependency detected!\n");
			exit(1);
		}

		if(cell->as.expr.status == UNEVALUATED){
			cell->as.expr.status = INPROGRESS;
			cell->as.expr.value = table_eval_expr(table, cell->as.expr.ast);
			cell->as.expr.status = EVALUATED;
		}
	}
}

int main(int argc, char **argv)
{

	if (argc < 2)
	{
		usage(stderr);
		fprintf(stderr, "ERROR: input file is not provided\n");
		exit(1);
	}

	const char *input_file_path = argv[1];
	size_t content_size = 0;
	// ! Read File
	const char *content = slurp_file(input_file_path, &content_size);
	// printf("\n %s \n", content);
	// printf("file size:- %ld \n", content_size);
	if (content == NULL)
	{
		fprintf(stderr, "ERROR: could not read file %s:%s \n", input_file_path, strerror(errno));
		exit(1);
	}

	String_View input = {
		.count = content_size,
		.data = content,
	};

	/** Get Dimensions */
	size_t row;
	size_t col;
	estimate_table_size(input, &row, &col);
	/* Put table into memory */
	Table table =table_alloc(row, col);
	/* Add data into table  */
	parse_table_from_content(&table, input);
	
	for(size_t row = 0; row < table.rows; ++row){
		for(size_t col = 0; col < table.cols; ++col){
			//printf("%s (%f)|",cell_kind_as_cstr(table_cell_at(&table, row, col)->kind),table_cell_at(&table,row,col)->as.number);
			// printf("CELL(%zu, %zu): ", row, col);
			Cell *cell = table_cell_at(&table, row, col);
			table_eval_cell(&table, cell);

		 
			//		switch(cell->kind){
			//	case CELL_KIND_TEXT:
			//	printf("TEXT(\""SV_Fmt"\")\n",SV_Arg(cell->as.text));
				//	break;
			//case CELL_KIND_NUMBER:
			//	printf("NUMBER(%lf)\n", cell->as.number);
			//break;
			//case CELL_KIND_EXPR:
			//	printf("EXPR:\n");
			//	dump_expr(stdout, cell->as.expr.ast, 1);
			//	break;
			//}

			switch (cell->kind){
			case CELL_KIND_TEXT:
				printf(SV_Fmt, SV_Arg(cell->as.text));
				break;
			case CELL_KIND_NUMBER:
				printf("%lf", cell->as.number);
				break;
			case CELL_KIND_EXPR:
				printf("%lf",cell->as.expr.value);
				break;
									
			}

			if(col < table.cols - 1){
				printf("|");
			}
			//		2:48
		}
		printf("\n");
	}
	
	return 0;
}
