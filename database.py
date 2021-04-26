import psycopg2
from psycopg2.extras import RealDictCursor

connectionString = """dbname='??????????????' 
                        user='??????????????' 
                        host='??????????????????????????????????????????????????' 
                        password='????????????????????????????????????????????????????????????????'"""

def dbInit():
    writeToDatabase("""create table if not exists cards (
        id serial PRIMARY KEY,
        cardid varchar(255) not null unique
    ); 
    create table if not exists home(
        temperature decimal,
        limittemp decimal
    );
        """)

def getScalarValueFromDatabase(query, arguments=None):
    if arguments is None:
        arguments = []
    with psycopg2.connect(connectionString) as connection:
        with connection.cursor() as cursor:
            try:
                cursor.execute(query, arguments)
                data = cursor.fetchone()
                if data is None:
                    return data, False
                return data[0], True
            except psycopg2.DatabaseError as error:
                print(error)
                return None, False


def writeToDatabase(query, arguments=[]):
    with psycopg2.connect(connectionString) as connection:
        with connection.cursor() as cursor:
            try:
                cursor.execute(query, arguments)
                return True, ""
            except psycopg2.DatabaseError as error:
                if "duplicate" in str(error):
                    return False, "Duplicate key error"
                print(error)
                return False, "Unidentified error occurred."


def getMultipleValuesFromDatabaseAsJson(query, arguments=[]):
    with psycopg2.connect(connectionString) as connection:
        with connection.cursor(cursor_factory=RealDictCursor) as cursor:
            try:
                cursor.execute(query, arguments)
                result = cursor.fetchall()
                if result:
                    for row in result:
                        row = dict(row)
                return True, result
            except psycopg2.DatabaseError as error:
                print(error)
                return False, None
