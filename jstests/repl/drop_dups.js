
var rt = new ReplTest( "drop_dups" );

m = rt.start( true );
s = rt.start( false );

var writeOption = { writeConcern: { w: 2, wtimeout: 3000 }};

am = m.getDB( "foo" );
as = s.getDB( "foo" );

function run( createInBackground ) {

    collName = "foo" + ( createInBackground ? "B" : "F" );
    
    am[collName].drop();
    am.blah.insert({ x: 1 }, writeOption);
    assert.soon( function(){
        return as.blah.findOne();
    }
               );

    var bulk = am[collName].initializeUnorderedBulkOp();
    for (var i = 0; i < 10; i++) {
        bulk.insert({ _id: i, x: Math.floor( i / 2 ) });
    }
    assert.writeOK(bulk.execute({ w: 2, wtimeout: 3000 }));

    am.runCommand( { "godinsert" : collName , obj : { _id : 100 , x : 20 } } );
    am.runCommand( { "godinsert" : collName , obj : { _id : 101 , x : 20 } } );

    as.runCommand( { "godinsert" : collName , obj : { _id : 101 , x : 20 } } );
    as.runCommand( { "godinsert" : collName , obj : { _id : 100 , x : 20 } } );
    
    assert.eq( as[collName].count() , am[collName].count() );
    
    function mymap(z) {
        return z._id + ":" + z.x + ",";
    }    
    
    am[collName].ensureIndex( { x : 1 } , { unique : true , dropDups : true , background : createInBackground  } );
    am.blah.insert({ x: 1 }, writeOption);

    assert.eq( 2 , am[collName].getIndexKeys().length , "A1 : " + createInBackground )
    if (!createInBackground) {
        assert.eq( 2 , as[collName].getIndexKeys().length , "A2 : " + createInBackground )
    }

    assert.eq( am[collName].find().sort( { _id : 1 } ).map(mymap) , 
               as[collName].find().sort( { _id : 1 } ).map(mymap) , "different things dropped on master and slave" );
    
    
}

run( false )
run( true )

rt.stop()
