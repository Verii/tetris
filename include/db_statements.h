#ifndef DB_STATEMENTS_H_
#define DB_STATEMENTS_H_

/* Scores: name, level, score, date */
const char create_scores[] =
	"CREATE TABLE Scores(name TEXT,level INT,score INT,date INT);";

const char insert_scores[] =
	"INSERT INTO Scores VALUES(\"%s\",%d,%d,%lu);";

const char select_scores[] =
	"SELECT * FROM Scores ORDER BY score DESC;";

/* State: name, score, lines, level, diff, date, width, height, spaces */
const char create_state[] =
	"CREATE TABLE State(name TEXT,score INT,lines INT,level INT,"
	"diff INT,date INT,width INT,height INT,spaces BLOB);";

const char insert_state[] =
	"INSERT INTO State VALUES(\"%s\",%d,%d,%d,%d,%lu,%d,%d,?);";

const char select_state[] =
	"SELECT * FROM State ORDER BY date DESC;";

/* Find the entry we just pulled from the database.
 * There's probably a simpler way than using two SELECT calls, but I'm a total
 * SQL noob, so ... */
const char select_state_rowid[] =
	"SELECT ROWID,date FROM State ORDER BY date DESC;";

/* Remove the entry pulled from the database. This lets us have multiple saves
 * in the database concurently.
 */
const char delete_state_rowid[] =
	"DELETE FROM State WHERE ROWID = ?;";

#endif /* DB_STATEMENTS_H_ */
