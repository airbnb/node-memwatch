const
should = require('should'),
memwatch = require('./');

function hrtimeInMicroseconds() {
  return process.hrtime.bigint() / 1000n;
}

describe('the library', function() {
  it('should export a couple functions', function(done) {
    should.exist(memwatch.gc);
    should.exist(memwatch.on);
    should.exist(memwatch.once);
    should.exist(memwatch.removeAllListeners);
    should.exist(memwatch.HeapDiff);
    done();
  });
});
describe('calling .gc()', function() {
  it('should cause a stats() event to be emitted', function(done) {
    let timeBeforeGc;

    memwatch.once('stats', function(s) {
      const timeAfterEvent = hrtimeInMicroseconds();

      s.should.be.object;

      (typeof timeBeforeGc).should.equal("bigint");
      timeAfterEvent.should.be.greaterThan(timeBeforeGc);

      s.gc_ts.should.be.a.Number();
      s.gc_ts.should.be.within(timeBeforeGc, timeAfterEvent);

      done();
    });

    timeBeforeGc = hrtimeInMicroseconds();
    memwatch.gc();
  });
});

describe('HeapDiff', function() {
  it('should detect allocations', function(done) {
    function LeakingClass() {};
    var arr = [];
    var hd = new memwatch.HeapDiff();
    for (var i = 0; i < 100; i++) arr.push(new LeakingClass());
    var diff = hd.end();
    (Array.isArray(diff.change.details)).should.be.ok;
    diff.change.details.should.be.an.instanceOf(Array);
    // find the LeakingClass elem
    var leakingReport;
    diff.change.details.forEach(function(d) {
      if (d.what === 'LeakingClass')
        leakingReport = d;
    });
    should.exist(leakingReport);
    ((leakingReport['+'] - leakingReport['-']) > 0).should.be.ok;
    done();

  });
});

describe('HeapDiff', function() {
  it('double end should throw', function(done) {
    var hd = new memwatch.HeapDiff();
    var arr = [];
    (function() { hd.end(); }).should.not.throw();
    (function() { hd.end(); }).should.throw();
    done();
  });
});

describe('improper HeapDiff allocation', function() {
  it('should throw an exception', function(done) {
    // equivalent to "new require('memwatch').HeapDiff()"
    // see issue #30
    (function() { new (memwatch.HeapDiff()); }).should.throw();
    done();
  });
});
