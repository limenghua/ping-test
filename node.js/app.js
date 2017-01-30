const ping = require ("net-ping");
const log4js = require("log4js");

log4js.configure("./log4js.json");
const logger = log4js.getLogger("console");

var session = ping.createSession ();

const fs = require('fs');
const file = ".\\targets.json";
const targets = JSON.parse(fs.readFileSync(file));

console.log(targets)

for(var target of targets){

    session.pingHost (target, function (error, target) {
        if (error){
          logger.info(target + ": " + error.toString ())
          console.log (target + ": " + error.toString ());
        }
        else{
          logger.info (target + ": Alive");
          console.log (target + ": Alive");
        }
    });
}
