/**
 * Copyright (C) 2013 Canonical, Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 3, as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranties of MERCHANTABILITY,
 * SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */


#include <gio/gio.h>
#include <gtest/gtest.h>
#include "dispatcher.h"
#include "upstart-app-launch-mock.h"

class AppIdTest : public ::testing::Test
{
	private:
		GTestDBus * testbus = NULL;
		GMainLoop * mainloop = NULL;

	protected:
		virtual void SetUp() {
			g_setenv("URL_DISPATCHER_CLICK_EXEC", CMAKE_SOURCE_DIR "/click-test.sh", TRUE);
			g_setenv("URL_DISPATCHER_TEST_CLICK_DIR", CMAKE_SOURCE_DIR "/click-data/", TRUE);

			testbus = g_test_dbus_new(G_TEST_DBUS_NONE);
			g_test_dbus_up(testbus);

			mainloop = g_main_loop_new(NULL, FALSE);
			dispatcher_init(mainloop);

			return;
		}

		virtual void TearDown() {
			dispatcher_shutdown();

			/* Clean up queued events */
			while (g_main_pending()) {
				g_main_iteration(TRUE);
			}

			g_main_loop_unref(mainloop);

			upstart_app_launch_mock_clear_last_app_id();

			g_test_dbus_down(testbus);
			g_object_unref(testbus);
			return;
		}
};

TEST_F(AppIdTest, BaseUrl)
{
	/* Good sanity check */
	dispatch_url("appid://com.test.good/app1/1.2.3");
	ASSERT_STREQ("com.test.good_app1_1.2.3", upstart_app_launch_mock_get_last_app_id());
	upstart_app_launch_mock_clear_last_app_id();

	/* No version at all */
	dispatch_url("appid://com.test.good/app1");
	ASSERT_TRUE(NULL == upstart_app_launch_mock_get_last_app_id());
	upstart_app_launch_mock_clear_last_app_id();
}

TEST_F(AppIdTest, WildcardUrl)
{
	/* Version wildcard */
	dispatch_url("appid://com.test.good/app1/current-user-version");
	ASSERT_STREQ("com.test.good_app1_1.2.3", upstart_app_launch_mock_get_last_app_id());
	upstart_app_launch_mock_clear_last_app_id();

	/* First app */
	dispatch_url("appid://com.test.good/first-listed-app/current-user-version");
	ASSERT_STREQ("com.test.good_app1_1.2.3", upstart_app_launch_mock_get_last_app_id());
	upstart_app_launch_mock_clear_last_app_id();

	/* Last app */
	dispatch_url("appid://com.test.good/last-listed-app/current-user-version");
	ASSERT_STREQ("com.test.good_app1_1.2.3", upstart_app_launch_mock_get_last_app_id());
	upstart_app_launch_mock_clear_last_app_id();

	/* Only app */
	dispatch_url("appid://com.test.good/only-listed-app/current-user-version");
	ASSERT_STREQ("com.test.good_app1_1.2.3", upstart_app_launch_mock_get_last_app_id());
	upstart_app_launch_mock_clear_last_app_id();

	/* Wild app fixed version */
	dispatch_url("appid://com.test.good/only-listed-app/1.2.3");
	ASSERT_STREQ("com.test.good_app1_1.2.3", upstart_app_launch_mock_get_last_app_id());
	upstart_app_launch_mock_clear_last_app_id();

	return;
}

TEST_F(AppIdTest, OrderingUrl)
{
	dispatch_url("appid://com.test.multiple/first-listed-app/current-user-version");
	ASSERT_STREQ("com.test.multiple_app-first_1.2.3", upstart_app_launch_mock_get_last_app_id());
	upstart_app_launch_mock_clear_last_app_id();

	dispatch_url("appid://com.test.multiple/last-listed-app/current-user-version");
	ASSERT_STREQ("com.test.multiple_app-last_1.2.3", upstart_app_launch_mock_get_last_app_id());
	upstart_app_launch_mock_clear_last_app_id();

	dispatch_url("appid://com.test.multiple/only-listed-app/current-user-version");
	ASSERT_TRUE(NULL == upstart_app_launch_mock_get_last_app_id());
	upstart_app_launch_mock_clear_last_app_id();

	return;
}
