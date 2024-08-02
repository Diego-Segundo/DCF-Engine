# DCF Engine Server

TODO:
    1. Separate dealing with new incomming connection to its owen function (done)
    2. Separate reading client data to its own function (done)
    3. Error resolved. Was sending the memory address of the first element in the array instead of the memory of the variable in main holding the first elements memory address. So, subsequente function create a local variable to hold the memory of the first element addresss causing changes to be removed once functions finished. So, mains variable never got the change applied. (done) 
    4. Use client asset valuation request to pull required DCF info from python server. (Check that info looks like a stock if not reject request)
    5. Build FCF function
    6. seperate helper/service functions from main directory to there own
 