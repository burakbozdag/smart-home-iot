from flask import Flask,flash, request, jsonify, render_template,Response,redirect,send_from_directory
import database as db
import json


app = Flask(__name__)
db.dbInit()

@app.route("/registercard", methods=["GET"])
def register():
    id = request.args.get("id")
    status,retval = db.writeToDatabase("insert into cards (cardid) values(%s)",[id])
    if status:
        return "1"
    return "0"

@app.route("/authorizecard", methods=["GET"])
def authorize():
    id = request.args.get("id")
    retval, status = db.getScalarValueFromDatabase("select * from cards where cardid = %s",[id])
    if retval:
        return "1"
    return "0"

@app.route("/settemplimit", methods=["POST"])
def setTemp():
    limit = float(request.form["limit"])
    status, retval = db.writeToDatabase("insert into home (temperature,limittemp) values(25,%s)",[limit])

@app.route("/temperature", methods=["GET"])
def temperature():
    temperature = request.args.get("temperature")
    limit = db.getScalarValueFromDatabase("select limittemp from home")
    if float(temperature) > float(limit):
        return "0"
    return "1"

@app.route("/")
def index():
    return render_template("index.html")


if __name__ == "__main__":
    app.run()
