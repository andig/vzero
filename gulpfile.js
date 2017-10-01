// include plug-ins
var gulp = require('gulp');
var gzip = require('gulp-gzip');
var del = require('del');


// settings
var base = './data/';

/**
 * Defaults
 */
gulp.task('default', function() {
	['.', 'css', 'js'].forEach(function(dir) {
		gulp.src(['!**/*.gz', base + dir + '/*'])
			.pipe(gzip())
			.pipe(gulp.dest(base + dir));
	});

	gulp.src(['!**/*.gz', base + 'icons/*.ico'])
		.pipe(gzip())
		.pipe(gulp.dest(base + 'icons'));
});

gulp.task('clean', function() {
	del.sync(base + '**/*.gz');
});
