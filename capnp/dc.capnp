@0xa1a1a1a1a1a1a1a1;

interface DC {
    hello @0 () -> (response :Text);
    sendBinary @1 (binary :Data) -> (response :Text);
    removeBinary @2 (filename :Text) -> (response :Text);
    someBinary @3 (name :Text) -> (response :Bool);
    executeBinary @4 (filename :Text, args :List(Text), stdinInput :Data) -> (response :Data);
    stopExecution @5 () -> (response :Bool);
    put @6 (key :Text, value :Text) -> (response :Text);
    append @7 (key :Text, value :Text) -> (response :Text);
    get @8 (key :Text) -> (response :Text);
    storeInTmp @9 (key :Text) -> (response :Text);
    deleteInTmp @10 (key :Text) -> (response :Text);
    deleteTmp @11 () -> (response :Text);
}

